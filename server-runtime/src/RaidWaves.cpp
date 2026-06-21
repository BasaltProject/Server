#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/RaiderEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using entity::Raid;
using entity::RaiderType;
using entity::RaidPos;
using player::PlayerSession;
using wire::common::Identifier;
namespace mth = wire::common::mth;

constexpr std::array<RaiderType, 5> kRaiderTypes{RaiderType::Vindicator, RaiderType::Evoker, RaiderType::Pillager,
                                                 RaiderType::Witch, RaiderType::Ravager};

/// parity: `getNumGroups(NORMAL)` and `getNumGroups(HARD)`.
constexpr std::int32_t kNormalGroups = 5;
constexpr std::int32_t kHardGroups = 7;
constexpr std::int32_t kEasyGroups = 3;

/// parity: `findRandomSpawnPos`.
constexpr float kRingPerSecond = 0.22F;
constexpr float kRingOffset = 0.24F;
constexpr float kRingRadius = 32.0F;
constexpr std::int32_t kMaxHeightDifference = 96;
constexpr std::int32_t kInsideVillageSeconds = 7;
constexpr std::int32_t kLoadedAround = 10;

/// parity: `playSound`.
constexpr double kHornDistance = 13.0;
constexpr double kHornRange = 64.0;
constexpr float kHornVolume = 64.0F;

[[nodiscard]] bool isSnow(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const Identifier* block = state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && block->path() == "snow";
}

[[nodiscard]] bool isAir(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    return state.has_value() && server::registry::blocks::isAir(*state);
}

[[nodiscard]] bool loadedAround(world::LoadedWorld& world, std::int32_t x, std::int32_t z) {
    for (std::int32_t chunkX = (x - kLoadedAround) >> 4; chunkX <= (x + kLoadedAround) >> 4; ++chunkX) {
        for (std::int32_t chunkZ = (z - kLoadedAround) >> 4; chunkZ <= (z + kLoadedAround) >> 4; ++chunkZ) {
            if (!world.state().isLoaded(chunkX, chunkZ)) {
                return false;
            }
        }
    }
    return true;
}

/// parity: `SpawnPlacementTypes.ON_GROUND.isSpawnPositionOk` for a ravager.
[[nodiscard]] bool ravagerFits(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const Identifier ravager = Identifier::vanilla("ravager");
    return world::naturalSpawner::canSpawnOnGround(world, x, y, z) &&
           world::naturalSpawner::isValidEmptySpawnBlock(world, ravager, x, y, z) &&
           world::naturalSpawner::isValidEmptySpawnBlock(world, ravager, x, y + 1, z);
}

} // namespace

std::optional<RaidPos> RaidSystem::findRandomSpawnPos(world::LoadedWorld& world, Raid& raid, std::int32_t maxTries) {
    const std::int32_t secondsRemaining = raid.cooldownTicks / 20;
    const float howFar = (kRingPerSecond * static_cast<float>(secondsRemaining)) - kRingOffset;
    const float startAngle = raid.random.nextFloat() * (std::numbers::pi_v<float> * 2.0F);
    for (std::int32_t attempt = 0; attempt < maxTries; ++attempt) {
        const float angle = startAngle + (std::numbers::pi_v<float> * static_cast<float>(attempt) / 8.0F);
        const std::int32_t x =
            raid.center[0] +
            mth::floor(static_cast<double>(mth::cos(static_cast<double>(angle)) * kRingRadius * howFar)) +
            (raid.random.nextInt(3) * mth::floor(static_cast<double>(howFar)));
        const std::int32_t z =
            raid.center[2] +
            mth::floor(static_cast<double>(mth::sin(static_cast<double>(angle)) * kRingRadius * howFar)) +
            (raid.random.nextInt(3) * mth::floor(static_cast<double>(howFar)));
        const server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
        if (chunk == nullptr) {
            continue;
        }
        const std::int32_t y = chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::WorldSurface);
        if (std::abs(y - raid.center[1]) > kMaxHeightDifference) {
            continue;
        }
        if (world.state().poi().isVillage(world::poi::BlockPos{x, y, z}) && secondsRemaining > kInsideVillageSeconds) {
            continue;
        }
        if (loadedAround(world, x, z) && world.state().isLoaded(x >> 4, z >> 4) &&
            (ravagerFits(world, x, y, z) || (isSnow(world, x, y - 1, z) && isAir(world, x, y, z)))) {
            return RaidPos{x, y, z};
        }
    }
    return std::nullopt;
}

void RaidSystem::spawnGroup(world::LoadedWorld& world, Raid& raid, const RaidPos& pos) {
    bool leaderSet = false;
    std::string spawned;
    std::int32_t leaderId = -1;
    const std::int32_t wave = raid.groupsSpawned + 1;
    raid.totalHealth = 0.0F;
    const bool bonusWave = raid.shouldSpawnBonusGroup();
    const std::string difficulty{world.difficulty()};
    for (const RaiderType type : kRaiderTypes) {
        const std::int32_t count = raid.spawnsFor(type, wave, difficulty, bonusWave);
        std::int32_t ravagersSpawned = 0;
        for (std::int32_t index = 0; index < count; ++index) {
            const std::string_view name = entity::raidWaves::typeName(type);
            const bool leads = !leaderSet && entity::raiderEquipment::canBeLeader(name);
            entity::ServerEntity* raider = spawnRaider(world, raid, name, wave, pos);
            if (raider == nullptr) {
                break;
            }
            spawned += (spawned.empty() ? "" : ",") + std::to_string(raider->entityId());
            if (leads) {
                raider->patrolLeader = true;
                setLeader(world, raid, wave, *raider);
                leaderSet = true;
                leaderId = raider->entityId();
            }
            if (type != RaiderType::Ravager) {
                continue;
            }
            std::string_view riderType;
            if (wave == kNormalGroups) {
                riderType = "pillager";
            } else if (wave >= kHardGroups) {
                riderType = ravagersSpawned == 0 ? "evoker" : "vindicator";
            }
            ++ravagersSpawned;
            if (riderType.empty()) {
                continue;
            }
            if (entity::ServerEntity* rider = spawnRaider(world, raid, riderType, wave, pos); rider != nullptr) {
                spawned += "," + std::to_string(rider->entityId());
                rider->vehicleId = raider->entityId();
                raider->passengerIds.push_back(rider->entityId());
            }
        }
    }
    raid.waveSpawnPos.reset();
    ++raid.groupsSpawned;
    updateBossbar(world, raid);
    world.raids().dirty = true;
    world.events().notify(&server::plugins::pluginEventTypes::raidWave, [&](server::plugins::PluginEvent& event) {
        plugins::hookEvents::position(event, world.name(), raid.center[0], raid.center[1], raid.center[2]);
        event.set("wave", static_cast<std::int64_t>(wave));
        event.set("raiders", spawned);
        event.set("leader_id", static_cast<std::int64_t>(leaderId));
    });
}

entity::ServerEntity* RaidSystem::spawnRaider(world::LoadedWorld& world, Raid& raid, std::string_view type,
                                              std::int32_t wave, const RaidPos& pos) {
    const std::optional<std::int32_t> raidId = world.raids().idOf(&raid);
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    // parity: `joinRaid` with a fresh raider.
    // the raid's own buffs, all before it is added to the world.
    entity::ServerEntity* raider =
        spawning_->spawnEventRaider(world, Identifier::vanilla(std::string{type}), pos[0] + 0.5, pos[1] + 1.0,
                                    pos[2] + 0.5, random, [&](entity::ServerEntity& fresh) {
                                        fresh.raidId = raidId;
                                        fresh.raidWave = wave;
                                        fresh.canJoinRaid = true;
                                        fresh.ticksOutsideRaid = 0;
                                        fresh.onGround = true;
                                        applyRaidBuffs(raid, fresh, wave);
                                    });
    if (raider == nullptr) {
        return nullptr;
    }
    raid.addWaveMob(wave, raider->entityId());
    raid.totalHealth += static_cast<float>(raider->health);
    updateBossbar(world, raid);
    return raider;
}

void RaidSystem::applyRaidBuffs(Raid& raid, entity::ServerEntity& raider, std::int32_t wave) {
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const std::string_view type = raider.type().path();
    if (type == "vindicator") {
        // parity: `Vindicator.applyRaidBuffs`.
        if (random.nextFloat() <= raid.enchantOdds()) {
            entity::raiderEquipment::holdInMainHand(raider, "iron_axe", "sharpness", wave > kNormalGroups ? 2 : 1);
        } else {
            entity::raiderEquipment::holdInMainHand(raider, "iron_axe");
        }
    } else if (type == "pillager") {
        // parity: `Pillager.applyRaidBuffs`.
        if (random.nextFloat() <= raid.enchantOdds() && wave > kEasyGroups) {
            entity::raiderEquipment::holdInMainHand(raider, "crossbow", "quick_charge", wave > kNormalGroups ? 2 : 1);
        }
    }
}

void RaidSystem::playHorn(world::LoadedWorld& world, Raid& raid, const RaidPos& origin) {
    const std::int64_t seed = raid.random.nextLong();
    const double raidX = origin[0] + 0.5;
    const double raidZ = origin[2] + 0.5;
    for (PlayerSession* player : playersIn(world)) {
        const double dx = raidX - player->x;
        const double dz = raidZ - player->z;
        const double distance = std::sqrt((dx * dx) + (dz * dz));
        if (distance > kHornRange && !raid.barPlayers.contains(player->entityId())) {
            continue;
        }
        player::playSoundFor(*player, "event.raid.horn", wire::protocol::play::SoundSource::Neutral,
                             player->x + (kHornDistance / distance * dx), player->y,
                             player->z + (kHornDistance / distance * dz), kHornVolume, 1.0F, seed);
    }
}

void RaidSystem::rewardHeroes(world::LoadedWorld& world, Raid& raid) {
    for (const wire::common::Uuid& hero : raid.heroes) {
        for (PlayerSession* player : playersIn(world)) {
            if (player->uuid() != hero || player->dead ||
                player->gamemode == wire::protocol::play::Gamemode::Spectator) {
                continue;
            }
            if (player->connection().addEffect) {
                player->connection().addEffect(world::mobEffects::heroOfTheVillage(), Raid::kHeroDurationTicks,
                                               raid.raidOmenLevel - 1);
            }
            if (hooks_.raidWon) {
                hooks_.raidWon(*player);
            }
        }
    }
}

} // namespace basalt::server::runtime
