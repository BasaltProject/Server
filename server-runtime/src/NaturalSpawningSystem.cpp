#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ChunkSimulationView.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/sched/RegionScheduler.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/LocalMobCaps.hpp>
#include <basalt/server/runtime/world/MobLocalDifficulty.hpp>
#include <basalt/server/runtime/world/SkeletonHorseTrap.hpp>
#include <basalt/server/runtime/world/SpawnPotential.hpp>
#include <basalt/server/runtime/world/StructureSpawnLists.hpp>
#include <basalt/server/runtime/world/VillageGolemSpawner.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::MobCategory;
using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;

constexpr double kChickenSearchHorizontal = 5.0;
constexpr double kChickenSearchVertical = 3.0;
constexpr float kFullRotation = 360.0F;

/// Every category a cap is kept for.
constexpr std::array<MobCategory, 7> kCappedCategories{MobCategory::Monster,
                                                       MobCategory::Creature,
                                                       MobCategory::Ambient,
                                                       MobCategory::Axolotls,
                                                       MobCategory::UndergroundWaterCreature,
                                                       MobCategory::WaterCreature,
                                                       MobCategory::WaterAmbient};

std::int64_t chunkKey(std::int32_t chunkX, std::int32_t chunkZ) {
    return (static_cast<std::int64_t>(chunkX) & 0xFFFFFFFFL) | (static_cast<std::int64_t>(chunkZ) << 32);
}

entity::Aabb entityBox(const ServerEntity& entity) {
    return entity::Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
}

entity::Aabb playerBox(double x, double y, double z, bool sneaking) {
    const std::array<double, 6> packed = world::blockPlacement::playerBox(x, y, z, sneaking);
    return entity::Aabb{packed[0], packed[1], packed[2], packed[3], packed[4], packed[5]};
}

bool spawnConditionsHold(world::LoadedWorld& world, const config::MobOverride& tweak,
                         const world::SpawnRequest& request, std::int32_t x, std::int32_t y, std::int32_t z,
                         const std::function<std::set<std::string>(const std::string&)>& biomeTag) {
    if (tweak.spawnEnabled == false || (tweak.spawnMinY.has_value() && y < *tweak.spawnMinY) ||
        (tweak.spawnMaxY.has_value() && y > *tweak.spawnMaxY)) {
        return false;
    }
    if (tweak.spawnMinLight.has_value() || tweak.spawnMaxLight.has_value()) {
        const std::int32_t light = world::worldLight::rawBrightness(world.state(), x, y, z, true, world.skyDarken());
        if ((tweak.spawnMinLight.has_value() && light < *tweak.spawnMinLight) ||
            (tweak.spawnMaxLight.has_value() && light > *tweak.spawnMaxLight)) {
            return false;
        }
    }
    if (tweak.spawnBiomes.has_value()) {
        const std::string biome = request.biome.has_value() ? request.biome->toString() : std::string{};
        return std::ranges::any_of(*tweak.spawnBiomes, [&](const std::string& entry) {
            if (entry.starts_with('#')) {
                return biomeTag && biomeTag(entry.substr(1)).contains(biome);
            }
            return entry == biome;
        });
    }
    return true;
}

} // namespace

NaturalSpawningSystem::NaturalSpawningSystem(player::PlayerRegistry& players, world::RealBiomeSpawns& biomeSpawns,
                                             ChunkSimulationView& chunkSimulation,
                                             const world::SpawnEquipmentFinalizer& equipment, Hooks hooks,
                                             Options options)
    : players_(&players), biomeSpawns_(&biomeSpawns), chunkSimulation_(&chunkSimulation), equipment_(&equipment),
      hooks_(std::move(hooks)), options_(std::move(options)) {}

NaturalSpawningSystem::WorldSpawners& NaturalSpawningSystem::spawnersFor(const world::LoadedWorld& world) {
    // Each spawner carries its countdown between ticks, so one set per world: a fresh one each tick never spawns.
    const auto found = spawners_.find(world.name());
    if (found != spawners_.end()) {
        return *found->second;
    }
    return *spawners_.emplace(world.name(), std::make_unique<WorldSpawners>()).first->second;
}

random::RandomSource NaturalSpawningSystem::freshRandom() const {
    return random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
}

bool NaturalSpawningSystem::spawnAllowed(const world::LoadedWorld& world, const Identifier& type, double x, double y,
                                         double z, std::string_view reason) const {
    return !hooks_.spawnAllowed || hooks_.spawnAllowed(world, type, x, y, z, reason);
}

ServerEntity* NaturalSpawningSystem::spawnMob(world::LoadedWorld& world, const Identifier& type, double x, double y,
                                              double z, float yaw, std::string_view reason,
                                              const std::function<void(ServerEntity&)>& initialize) {
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(type);
    if (entry == nullptr || !spawnAllowed(world, type, x, y, z, reason)) {
        return nullptr;
    }
    auto entity = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), type,
                                                 entry->protocolId, x, y, z);
    entity->yaw = yaw;
    entity->spawnReason = std::string{reason};
    if (initialize) {
        initialize(*entity);
    }
    ServerEntity& added = world.state().spawnEntity(std::move(entity));
    if (hooks_.spawned) {
        hooks_.spawned(world, added, reason);
    }
    return &added;
}

ServerEntity* NaturalSpawningSystem::spawnSkeletonTrap(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z) {
    return spawnMob(world, Identifier::vanilla("skeleton_horse"), static_cast<double>(x), static_cast<double>(y),
                    static_cast<double>(z), 0.0F, "event", [](ServerEntity& horse) {
                        horse.skeletonTrap = true;
                        horse.skeletonTrapTime = 0;
                        horse.age = 0;
                    });
}

bool NaturalSpawningSystem::activateSkeletonTrap(world::LoadedWorld& world, ServerEntity& horse,
                                                 random::RandomSource& random) {
    if (horse.type().path() != "skeleton_horse" || !horse.skeletonTrap || horse.removed) {
        return false;
    }
    random::RandomSource horseRandom = freshRandom();
    const world::MobSpawnForest forest = world::skeletonHorseTrap::activate(
        world, horse, random, horseRandom, [this] { return freshRandom(); }, *equipment_);
    materializeForest(world, forest, "triggered", true, [](ServerEntity& entity) {
        entity.hurtCooldown = world::skeletonHorseTrap::kActivationInvulnerableTicks;
    });
    return true;
}

bool NaturalSpawningSystem::summonVillageGolem(world::LoadedWorld& world, const ServerEntity& villager,
                                               random::RandomSource& random) {
    const std::optional<std::array<std::int32_t, 3>> position =
        world::villageGolemSpawner::findSpawnPosition(world, villager, random);
    if (!position.has_value()) {
        return false;
    }
    const world::SpawnRequest request{
        .type = Identifier::vanilla("iron_golem"),
        .x = (*position)[0] + 0.5,
        .y = static_cast<double>((*position)[1]),
        .z = (*position)[2] + 0.5,
        .yaw = random.nextFloat() * kFullRotation,
        .biome = world::naturalSpawner::biomeAt(world, (*position)[0], (*position)[1], (*position)[2])};
    world::MobSpawnGroupData group;
    return materializeSpawnForest(world, request, group, (*position)[0], (*position)[1], (*position)[2], random,
                                  world::MobSpawnReason::MobSummoned, "mob_summoned") != nullptr;
}

void NaturalSpawningSystem::tickSpawning(world::LoadedWorld& world, const sched::Region& region) {
    // Global and local caps are one world-wide snapshot: per region, the first distant player consumed everyone's cap.
    if (!region.isPrimary() || !world.gameRules().get(world::gameRules::kSpawnMobs)) {
        return;
    }
    random::RandomSource random = freshRandom();
    std::vector<PlayerSession*> worldPlayers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            worldPlayers.push_back(player);
        }
    }
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : worldPlayers) {
        if (player->gamemode != Gamemode::Spectator) {
            watchers.push_back(player);
        }
    }
    if (!watchers.empty()) {
        tickNaturalSpawning(world, watchers, random);
    }
    const bool spawnEnemies =
        world.difficulty() != "peaceful" && world.gameRules().get(world::gameRules::kSpawnMonsters);
    WorldSpawners& spawners = spawnersFor(world);
    spawners.phantom.tick(world, spawnEnemies, worldPlayers, random, [&](const world::SpawnRequest& request) {
        ServerEntity* phantom = materializeNaturalForest(
            world, request, static_cast<std::int32_t>(std::floor(request.x)),
            static_cast<std::int32_t>(std::floor(request.y)), static_cast<std::int32_t>(std::floor(request.z)), random);
        if (phantom != nullptr && request.spawnedFor.has_value()) {
            // parity: `PhantomSpawner`'s `setSpawningEntity`, kept the way Paper saves it.
            if (!phantom->sourceNbt.has_value()) {
                phantom->sourceNbt = wire::nbt::NbtCompound{};
            }
            const wire::common::Uuid& who = *request.spawnedFor;
            phantom->sourceNbt->putIntArray("Paper.SpawningEntity",
                                            {static_cast<std::int32_t>(static_cast<std::uint32_t>(who.high() >> 32)),
                                             static_cast<std::int32_t>(static_cast<std::uint32_t>(who.high())),
                                             static_cast<std::int32_t>(static_cast<std::uint32_t>(who.low() >> 32)),
                                             static_cast<std::int32_t>(static_cast<std::uint32_t>(who.low()))});
        }
    });
    spawners.patrol.tick(world, spawnEnemies, worldPlayers, random, [&](const world::PatrolSpawnRequest& request) {
        return materializePatrolMember(world, request, random) != nullptr;
    });
    spawners.cat.tick(world, worldPlayers, random,
                      [&](const world::CatSpawnRequest& request) { materializeCat(world, request, random); });
    spawners.villageSiege.tick(
        world, spawnEnemies, worldPlayers, random,
        [&](const world::VillageSiegeSpawnRequest& request) { materializeVillageSiegeZombie(world, request, random); });
    spawners.wanderingTrader.tick(
        world, worldPlayers, random,
        [&](const world::WanderingTraderSpawnRequest& request) -> std::optional<std::int32_t> {
            ServerEntity* trader = materializeWanderingTrader(world, request, random);
            return trader == nullptr ? std::nullopt : std::optional{trader->entityId()};
        },
        [&](const world::TraderLlamaSpawnRequest& request, std::int32_t traderId) {
            materializeTraderLlama(world, request, traderId, random);
        });
}

void NaturalSpawningSystem::tickNaturalSpawning(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                                random::RandomSource& random) {
    std::vector<world::MobCapPlayer> capPlayers;
    capPlayers.reserve(watchers.size());
    for (const PlayerSession* player : watchers) {
        capPlayers.push_back(world::MobCapPlayer{player->entityId(), player->x, player->z});
    }
    const config::MobSettings& mobs = world.mobSettings();
    const world::LoadedWorld::SpawnTunables& tunables = world.spawnTunables();
    world::LocalMobCaps::Limits limits{};
    for (const MobCategory category : kCappedCategories) {
        const auto slot = static_cast<std::size_t>(category);
        limits[slot] = tunables.limits[slot] >= 0 ? tunables.limits[slot] : mobs.caps[slot];
    }
    world::LocalMobCaps localCaps{std::move(capPlayers), limits, mobs.spawnRangeChunks};

    std::vector<std::array<std::int32_t, 2>> chunks;
    for (const std::array<std::int32_t, 2>& chunk : chunkSimulation_->simulatedChunks(world)) {
        if (localCaps.isSpawnCandidate(chunk[0], chunk[1])) {
            chunks.push_back(chunk);
        }
    }
    if (chunks.empty()) {
        return;
    }
    std::set<std::int64_t> spawnableChunks;
    for (const std::array<std::int32_t, 2>& chunk : chunks) {
        spawnableChunks.insert(chunkKey(chunk[0], chunk[1]));
    }
    world::StructureSpawnLists structureSpawnLists{world};

    // parity: `NaturalSpawner.createState`.
    std::vector<ServerEntity*> alive;
    for (ServerEntity* entity : world.state().entities()) {
        if (!entity->removed && !entity::mobPersistence::keeps(*entity)) {
            alive.push_back(entity);
        }
    }
    std::vector<MobCategory> categories = world::naturalSpawner::categoriesFor(world.difficulty(), world.gameTime());
    std::map<MobCategory, std::int32_t> caps;
    std::map<MobCategory, std::int32_t> counts;
    for (const MobCategory category : kCappedCategories) {
        const auto slot = static_cast<std::size_t>(category);
        caps[category] = limits[slot] * static_cast<std::int32_t>(chunks.size()) / world::naturalSpawner::kCapChunks;
        counts[category] = 0;
        const std::int32_t vanillaInterval = entity::mobCategoryFacts(category).persistent ? 400 : 1;
        const std::int32_t every =
            tunables.ticksPer[slot] >= 0 ? tunables.ticksPer[slot] : mobs.spawnIntervalTicks[slot];
        if (tunables.ticksPer[slot] >= 0 || every != vanillaInterval) {
            std::erase(categories, category);
            const bool peaceful = world.difficulty() == "peaceful" && !entity::mobCategoryFacts(category).friendly;
            if (every > 0 && world.gameTime() % every == 0 && !peaceful) {
                categories.push_back(category);
            }
        }
    }
    std::erase_if(categories, [&tunables](const MobCategory category) {
        return entity::mobCategoryFacts(category).friendly ? !tunables.allowAnimals : !tunables.allowMonsters;
    });
    world::SpawnPotential spawnPotential;
    for (const ServerEntity* entity : alive) {
        const auto blockX = static_cast<std::int32_t>(std::floor(entity->x));
        const auto blockY = static_cast<std::int32_t>(std::floor(entity->y));
        const auto blockZ = static_cast<std::int32_t>(std::floor(entity->z));
        const std::optional<Identifier> biome = world::naturalSpawner::biomeAt(world, blockX, blockY, blockZ);
        if (biome.has_value()) {
            const std::optional<world::MobSpawnCost> cost = biomeSpawns_->costFor(*biome, entity->type());
            if (cost.has_value()) {
                spawnPotential.add(blockX, blockY, blockZ, cost->charge);
            }
        }
        const std::optional<MobCategory> category = entity::mobCategoryByName(entity::mobs::categoryOf(entity->type()));
        if (!category.has_value()) {
            continue;
        }
        counts[*category]++;
        localCaps.add(*category, blockX >> 4, blockZ >> 4);
    }
    // parity: `ServerChunkCache` shuffles once.
    for (std::size_t index = chunks.size(); index > 1; index--) {
        const auto swap = static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(index)));
        std::swap(chunks[index - 1], chunks[swap]);
    }
    for (const std::array<std::int32_t, 2>& chunk : chunks) {
        for (const MobCategory category : categories) {
            if ((!mobs.perPlayerMobSpawns && counts[category] >= caps[category]) ||
                !localCaps.canSpawn(category, chunk[0], chunk[1])) {
                continue;
            }
            const world::naturalSpawner::Hooks hooks{
                .entriesFor =
                    [&](const Identifier& biome, std::int32_t x, std::int32_t y, std::int32_t z) {
                        const std::optional<std::vector<world::SpawnerEntry>> structured =
                            structureSpawnLists.listAt(entity::mobCategoryFacts(category).categoryName, x, y, z);
                        std::vector<world::SpawnerEntry> list =
                            structured.has_value()
                                ? *structured
                                : biomeSpawns_->listFor(biome, entity::mobCategoryFacts(category).categoryName);
                        for (world::SpawnerEntry& entry : list) {
                            const config::MobOverride* tweak = mobs.overrideFor(entry.type.path());
                            if (tweak != nullptr && tweak->spawnWeight.has_value()) {
                                entry.weight = *tweak->spawnWeight;
                            }
                        }
                        std::erase_if(list, [](const world::SpawnerEntry& entry) { return entry.weight <= 0; });
                        return list;
                    },
                .playerDistanceSquared = [&watchers](double x, double y, double z) -> std::optional<double> {
                    std::optional<double> nearest;
                    for (const PlayerSession* player : watchers) {
                        const double dx = player->x - x;
                        const double dy = player->y - y;
                        const double dz = player->z - z;
                        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
                        if (!nearest.has_value() || distance < *nearest) {
                            nearest = distance;
                        }
                    }
                    return nearest;
                },
                .canSpawnEntitiesInChunk =
                    [&spawnableChunks](std::int32_t chunkX, std::int32_t chunkZ) {
                        return spawnableChunks.contains(chunkKey(chunkX, chunkZ));
                    },
                .requestAccepted =
                    [&](const world::SpawnRequest& request) {
                        const auto blockX = static_cast<std::int32_t>(std::floor(request.x));
                        const auto blockY = static_cast<std::int32_t>(std::floor(request.y));
                        const auto blockZ = static_cast<std::int32_t>(std::floor(request.z));
                        const std::optional<Identifier> biome =
                            world::naturalSpawner::biomeAt(world, blockX, blockY, blockZ);
                        std::optional<world::MobSpawnCost> cost;
                        if (biome.has_value()) {
                            cost = biomeSpawns_->costFor(*biome, request.type);
                        }
                        if (cost.has_value() && !spawnPotential.allows(blockX, blockY, blockZ, *cost)) {
                            return false;
                        }
                        if (const config::MobOverride* tweak = mobs.overrideFor(request.type.path());
                            tweak != nullptr &&
                            !spawnConditionsHold(world, *tweak, request, blockX, blockY, blockZ, options_.biomeTag)) {
                            return false;
                        }
                        if (materializeNaturalForest(world, request, blockX, blockY, blockZ, random) == nullptr) {
                            return false;
                        }
                        if (cost.has_value()) {
                            spawnPotential.add(blockX, blockY, blockZ, cost->charge);
                        }
                        const MobCategory actual =
                            entity::mobCategoryByName(entity::mobs::categoryOf(request.type)).value_or(category);
                        counts[actual]++;
                        localCaps.add(actual, blockX >> 4, blockZ >> 4);
                        return true;
                    }};
            static_cast<void>(world::naturalSpawner::spawnForChunk(world, chunk[0], chunk[1], category, hooks, random));
        }
    }
}

world::MobSpawnEnvironment NaturalSpawningSystem::spawnEnvironment(world::LoadedWorld& world) {
    return world::MobSpawnEnvironment{
        .nearbyUnriddenChicken = [&world](const world::MobSpawnNode& zombie) -> std::optional<std::int32_t> {
            const double scale = zombie.data().getBoolean("IsBaby") ? 0.5 : 1.0;
            const entity::Aabb source = entity::Aabb::of(
                zombie.x(), zombie.y(), zombie.z(), server::registry::entityTypeShapes::widthOf(zombie.type()) * scale,
                server::registry::entityTypeShapes::heightOf(zombie.type()) * scale);
            const entity::Aabb search{source.minX - kChickenSearchHorizontal, source.minY - kChickenSearchVertical,
                                      source.minZ - kChickenSearchHorizontal, source.maxX + kChickenSearchHorizontal,
                                      source.maxY + kChickenSearchVertical,   source.maxZ + kChickenSearchHorizontal};
            for (const ServerEntity* candidate : world.state().entities()) {
                if (candidate->removed || candidate->health <= 0.0 || candidate->type().path() != "chicken" ||
                    candidate->vehicleId.has_value() || !candidate->passengerIds.empty()) {
                    continue;
                }
                if (entityBox(*candidate).intersects(search)) {
                    return candidate->entityId();
                }
            }
            return std::nullopt;
        },
        .canSpawnEntity =
            [this, &world](const Identifier& type, double x, double y, double z) {
                const entity::Aabb box = entity::Aabb::of(x, y, z, server::registry::entityTypeShapes::widthOf(type),
                                                          server::registry::entityTypeShapes::heightOf(type));
                const auto blockAt = [&world](std::int32_t bx, std::int32_t by, std::int32_t bz) {
                    return world.state().blockState(bx, by, bz);
                };
                if (entity::entityCollision::collides(blockAt, box)) {
                    return false;
                }
                for (const ServerEntity* candidate : world.state().entities()) {
                    if (!candidate->removed && entityBox(*candidate).intersects(box)) {
                        return false;
                    }
                }
                for (const PlayerSession* player : players_->snapshot()) {
                    if (player->worldName != world.name() || player->gamemode == Gamemode::Spectator) {
                        continue;
                    }
                    if (playerBox(player->x, player->y, player->z, player->sneaking).intersects(box)) {
                        return false;
                    }
                }
                return true;
            }};
}

float NaturalSpawningSystem::specialMultiplierAt(world::LoadedWorld& world, std::int32_t blockX,
                                                 std::int32_t blockZ) const {
    const server::world::Chunk* chunk = world.state().chunkIfLoaded(blockX >> 4, blockZ >> 4);
    const std::int64_t inhabitedTime = chunk == nullptr ? 0 : chunk->inhabitedTime();
    const float moonBrightness = world::naturalSpawner::surfaceSlimeChance(world.clock().totalTicks()) * 2.0F;
    return world::mobLocalDifficulty::specialMultiplier(world.difficulty(), world.gameTime(), inhabitedTime,
                                                        moonBrightness);
}

} // namespace basalt::server::runtime
