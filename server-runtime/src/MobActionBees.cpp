#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/poi/PoiManager.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using Pos = std::array<std::int32_t, 3>;
namespace beehives = world::beehives;
namespace mth = wire::common::mth;
namespace reg = server::registry;

/// parity: `Timelines.OVERWORLD_DAY`'s `BEES_STAY_IN_HIVE` track, on from 12542 to 23460.
constexpr std::int64_t kHiveNightStart = 12542;
constexpr std::int64_t kHiveNightEnd = 23460;

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = reg::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::optional<std::int32_t> withProperty(std::int32_t stateId, const std::string& name,
                                                       const std::string& value) {
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::nullopt : reg::blockStateDefinitions::stateIdOf(*block, {{name, value}}, stateId);
}

[[nodiscard]] server::world::ChunkBlockEntity* hiveEntity(world::LoadedWorld& world, const Pos& at) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    return chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
}

[[nodiscard]] bool isHiveState(std::int32_t stateId) {
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && beehives::isHive(block->path());
}

/// parity: `Direction.getStepX` and `getStepZ` of a hive's `facing`.
[[nodiscard]] std::array<std::int32_t, 2> stepOf(std::int32_t stateId) {
    const std::string facing = propertyOf(stateId, "facing");
    if (facing == "south") {
        return {0, 1};
    }
    if (facing == "west") {
        return {-1, 0};
    }
    if (facing == "east") {
        return {1, 0};
    }
    return {0, -1};
}

void markDirty(world::LoadedWorld& world, const Pos& at) {
    if (server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4); chunk != nullptr) {
        chunk->markDirty();
    }
}

/// parity: `EnvironmentAttributes.BEES_STAY_IN_HIVE`: any rain at all, then the overworld's night.
[[nodiscard]] bool staysInHive(const world::LoadedWorld& world) {
    if (world.weather().rainLevel() > 0.0F) {
        return true;
    }
    if (!config::dimensionFacts(world.declaration().dimension).hasDayTimeline) {
        return false;
    }
    const std::int64_t time = ((world.clock().totalTicks() % 24000) + 24000) % 24000;
    return time >= kHiveNightStart && time < kHiveNightEnd;
}

} // namespace

void MobActionSystem::tickBee(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers, ServerEntity& bee,
                              random::RandomSource& random) {
    entity::bees::BeeState& state = entity::bees::stateOf(bee, random);
    if (state.legacyAngerTime.has_value()) {
        state.angerEndTime = world.gameTime() + *std::exchange(state.legacyAngerTime, std::nullopt);
    }
    if (!bee.noAi) {
        // parity: `customServerAiStep`: water, the sting's slow death, nectarless time and anger.
        const auto hurt = [&](float amount, std::string_view cause, const wire::common::Identifier& type) {
            const entity::entityCombat::Result result =
                entity::entityCombat::hurt(bee, amount, std::nullopt, std::nullopt, hooks_.combat, cause);
            if (result.landed) {
                combat_->announceEntityHit(watchers, bee, type, -1, result.died);
            }
        };
        state.underWaterTicks = entity::ai::inWater(world.state(), bee) ? state.underWaterTicks + 1 : 0;
        if (state.underWaterTicks > 20) {
            hurt(1.0F, "drown", world::damageTypes::kDrown);
        }
        if (state.hasStung) {
            ++state.timeSinceSting;
            if (state.timeSinceSting % 5 == 0 &&
                random.nextInt(std::clamp(entity::bees::kStingDeathCountdown - state.timeSinceSting, 1,
                                          entity::bees::kStingDeathCountdown)) == 0) {
                hurt(static_cast<float>(bee.health), "generic", world::damageTypes::kGeneric);
            }
        }
        if (!state.hasNectar) {
            ++state.ticksWithoutNectar;
        }
        updateBeeAnger(world, bee, random);
    }
    // parity: the rest of `Bee.aiStep`: three countdowns, the roll, and a hive that went away.
    state.stayOutOfHive = std::max(state.stayOutOfHive - 1, 0);
    state.hiveCooldown = std::max(state.hiveCooldown - 1, 0);
    state.flowerCooldown = std::max(state.flowerCooldown - 1, 0);
    bool close = false;
    if (bee.targetId.has_value()) {
        for (PlayerSession* player : watchers) {
            if (player->entityId() == *bee.targetId) {
                close = (player->x - bee.x) * (player->x - bee.x) + (player->y - bee.y) * (player->y - bee.y) +
                            (player->z - bee.z) * (player->z - bee.z) <
                        4.0;
            }
        }
        if (const ServerEntity* other = world.state().entity(*bee.targetId); other != nullptr) {
            close = (other->x - bee.x) * (other->x - bee.x) + (other->y - bee.y) * (other->y - bee.y) +
                        (other->z - bee.z) * (other->z - bee.z) <
                    4.0;
        }
    }
    state.rolling = entity::bees::isAngry(state, world.gameTime()) && !state.hasStung && close;
    if (bee.tickCount % 20 == 0 && state.hive.has_value() &&
        (!entity::bees::closerThan(bee, *state.hive, entity::bees::kTooFarDistance) ||
         !hiveAt(bee, *state.hive).hive)) {
        state.hive.reset();
    }
}

void MobActionSystem::updateBeeAnger(world::LoadedWorld& world, ServerEntity& bee, random::RandomSource& random) {
    entity::bees::BeeState& state = *bee.bee;

    struct Living {
        wire::common::Uuid uuid;
        bool alive = false;
        bool player = false;
        bool exempt = false;
    };

    const bool peaceful = world.difficulty() == "peaceful";
    const auto resolveId = [&](std::int32_t id) -> std::optional<Living> {
        for (PlayerSession* player : players_->snapshot()) {
            if (player->entityId() == id && player->worldName == world.name()) {
                const bool exempt = player->gamemode == wire::protocol::play::Gamemode::Creative ||
                                    player->gamemode == wire::protocol::play::Gamemode::Spectator;
                return Living{.uuid = player->uuid(), .alive = !player->dead, .player = true, .exempt = exempt};
            }
        }
        const ServerEntity* other = world.state().entity(id);
        if (other == nullptr) {
            return std::nullopt;
        }
        return Living{.uuid = other->uuid(), .alive = !other->removed && !other->dying && other->health > 0.0};
    };
    const std::optional<wire::common::Uuid> persistent = state.angryAt;
    const std::optional<Living> previous = bee.targetId.has_value() ? resolveId(*bee.targetId) : std::nullopt;
    if (previous.has_value() && !previous->alive && persistent == previous->uuid && !previous->player) {
        entity::bees::stopBeingAngry(bee, state);
        return;
    }
    if (previous.has_value() && previous->alive) {
        if (!persistent.has_value() || *persistent != previous->uuid) {
            state.angryAt = previous->uuid;
            entity::bees::startAngerTimer(state, world.gameTime(), random);
        }
    }
    if (persistent.has_value() && !entity::bees::isAngry(state, world.gameTime())) {
        entity::bees::stopBeingAngry(bee, state);
    }
    // parity: a creative or spectating player, or a peaceful world, calls the anger off.
    if (persistent.has_value()) {
        for (PlayerSession* player : players_->snapshot()) {
            if (player->uuid() == *persistent && player->worldName == world.name() &&
                (peaceful || player->gamemode == wire::protocol::play::Gamemode::Creative ||
                 player->gamemode == wire::protocol::play::Gamemode::Spectator)) {
                entity::bees::stopBeingAngry(bee, state);
            }
        }
    }
}

void MobActionSystem::beeStung(world::LoadedWorld& world, ServerEntity& bee, PlayerSession* player, ServerEntity* mob) {
    random::RandomSource random = freshRandom();
    entity::bees::BeeState& state = entity::bees::stateOf(bee, random);
    // parity: ten seconds of poison on normal and eighteen on hard.
    std::int32_t seconds = 0;
    if (world.difficulty() == "normal") {
        seconds = 10;
    } else if (world.difficulty() == "hard") {
        seconds = 18;
    }
    if (seconds > 0) {
        if (player != nullptr) {
            applyEffect(player->entityId(), "poison", seconds * 20, 0, bee.entityId());
        } else if (mob != nullptr) {
            villagersIn(world).addEffect(*mob, "poison", seconds * 20, 0);
        }
    }
    // parity: `Bee.doHurtTarget` leaves its stinger in whatever it stung.
    if (mob != nullptr) {
        ++mob->stingerCount;
    } else if (player != nullptr) {
        ++player->stingerCount;
    }
    state.hasStung = true;
    entity::bees::stopBeingAngry(bee, state);
    playSound(world, bee, "entity.bee.sting");
}

MobActionSystem::HiveView MobActionSystem::hiveAt(const ServerEntity& bee, const Pos& pos) {
    world::LoadedWorld* world = worldOf(bee);
    if (world == nullptr) {
        return {};
    }
    const std::optional<std::int32_t> state = world->state().blockState(pos[0], pos[1], pos[2]);
    if (!state.has_value()) {
        return {};
    }
    HiveView view{.loaded = true};
    const server::world::ChunkBlockEntity* entity = hiveEntity(*world, pos);
    if (!isHiveState(*state) || entity == nullptr) {
        return view;
    }
    view.hive = true;
    view.full = beehives::isFull(entity->data());
    view.fireNearby = beehives::isFireNearby(world->state(), pos);
    return view;
}

void MobActionSystem::enterHive(ServerEntity& bee, const Pos& hive) {
    world::LoadedWorld* world = worldOf(bee);
    server::world::ChunkBlockEntity* entity = world == nullptr ? nullptr : hiveEntity(*world, hive);
    if (entity == nullptr || beehives::occupantCount(entity->data()) >= beehives::kMaxOccupants || bee.removed) {
        return;
    }
    random::RandomSource random = freshRandom();
    // parity: `Occupant.of`: the bee saved, less the tags a hive does not keep.
    wire::nbt::NbtCompound saved = entity::entityNbt::encode(bee);
    entity::bees::stripHiveTags(saved);
    beehives::addOccupant(entity->data(), std::move(saved));
    const entity::bees::BeeState* state = bee.bee.get();
    if (state != nullptr && state->flower.has_value() &&
        (!beehives::flowerOf(entity->data()).has_value() || random.nextBoolean())) {
        beehives::setFlower(entity->data(), *state->flower);
    }
    const std::int32_t stateId = world->state().blockState(hive[0], hive[1], hive[2]).value_or(0);
    broadcaster_->sendEffects(watchersOf(*world), hive[0], hive[1], hive[2], stateId,
                              {world::blockEffect::Sound{.name = "block.beehive.enter"}});
    world::vibrations::emit(world->state(), "block_change", hive[0] + 0.5, hive[1] + 0.5, hive[2] + 0.5,
                            world->gameTime(), {.entity = bee.uuid()});
    // parity: `discard`, gone into the hive without dying.
    bee.removed = true;
    markDirty(*world, hive);
    // parity: `BeehiveBlockEntity.setChanged`, which lets everybody out again if fire is near.
    if (beehives::isFireNearby(world->state(), hive)) {
        emptyHive(*world, hive, stateId, nullptr, beehives::Release::Emergency);
    }
}

std::vector<Pos> MobActionSystem::beeHomesNear(const ServerEntity& bee, std::int32_t radius) {
    std::vector<Pos> homes;
    world::LoadedWorld* world = worldOf(bee);
    if (world == nullptr) {
        return homes;
    }
    std::vector<world::poi::PoiRecord> records;
    world->state().poi().inRange(world::poi::poiTypes::kBeeHome,
                                 world::poi::BlockPos{mth::floor(bee.x), mth::floor(bee.y), mth::floor(bee.z)}, radius,
                                 world::poi::Occupancy::kAny, records);
    homes.reserve(records.size());
    for (const world::poi::PoiRecord& record : records) {
        homes.push_back({record.pos.x, record.pos.y, record.pos.z});
    }
    return homes;
}

bool MobActionSystem::beesStayInHive(const ServerEntity& bee) {
    const world::LoadedWorld* world = worldOf(bee);
    return world != nullptr && staysInHive(*world);
}

bool MobActionSystem::raining(const ServerEntity& bee) {
    const world::LoadedWorld* world = worldOf(bee);
    return world != nullptr && world->weather().isRaining();
}

bool MobActionSystem::growBeeCrop(ServerEntity& bee, const Pos& pos) {
    world::LoadedWorld* world = worldOf(bee);
    if (world == nullptr) {
        return false;
    }
    const std::optional<std::int32_t> below = world->state().blockState(pos[0], pos[1], pos[2]);
    const wire::common::Identifier* block = below.has_value() ? reg::blockStateDefinitions::blockOf(*below) : nullptr;
    if (block == nullptr || !blockTagged("minecraft:bee_growables", *block).value_or(false)) {
        return false;
    }
    const std::string_view path = block->path();
    const std::string ageText = propertyOf(*below, "age");
    const std::int32_t age = ageText.empty() ? 0 : std::stoi(ageText);
    std::optional<std::int32_t> grown;
    // parity: `CropBlock.getStateForAge`, `StemBlock.AGE`, `SweetBerryBushBlock.AGE` and cave vines'
    // bone meal, which is their berries.
    if (path == "wheat" || path == "carrots" || path == "potatoes") {
        grown = age < 7 ? withProperty(*below, "age", std::to_string(age + 1)) : std::nullopt;
    } else if (path == "beetroots") {
        grown = age < 3 ? withProperty(*below, "age", std::to_string(age + 1)) : std::nullopt;
    } else if (path == "torchflower_crop") {
        grown = age < 1 ? withProperty(*below, "age", "1")
                        : reg::blockStateDefinitions::stateIdOf(wire::common::Identifier::vanilla("torchflower"), {});
    } else if (path == "melon_stem" || path == "pumpkin_stem") {
        grown = age < 7 ? withProperty(*below, "age", std::to_string(age + 1)) : std::nullopt;
    } else if (path == "sweet_berry_bush") {
        grown = age < 3 ? withProperty(*below, "age", std::to_string(age + 1)) : std::nullopt;
    } else if (path == "cave_vines" || path == "cave_vines_plant") {
        grown = propertyOf(*below, "berries") == "true" ? std::nullopt : withProperty(*below, "berries", "true");
    }
    if (!grown.has_value()) {
        return false;
    }
    entity::villager::VillagerWorld& blocks = villagersIn(*world);
    const world::poi::BlockPos at{pos[0], pos[1], pos[2]};
    // parity: `LevelEvent.PARTICLES_BEE_GROWTH`, fifteen of them.
    blocks.levelEvent(bee, 2011, at, 15);
    return blocks.setBlock(bee, at, *grown);
}

bool MobActionSystem::releaseOccupant(world::LoadedWorld& world, const Pos& hive, std::int32_t stateId,
                                      const wire::nbt::NbtCompound& occupant, beehives::Release release,
                                      const std::optional<Pos>& flower, std::vector<ServerEntity*>* spawned,
                                      std::vector<world::BlockChange>* pending) {
    const bool emergency = release == beehives::Release::Emergency;
    if (!emergency && staysInHive(world)) {
        return false;
    }
    const std::array<std::int32_t, 2> step = stepOf(stateId);
    const std::optional<std::int32_t> front = world.state().blockState(hive[0] + step[0], hive[1], hive[2] + step[1]);
    const bool frontBlocked = front.has_value() && !reg::blockShapes::isEmptyShape(*front);
    if (frontBlocked && !emergency) {
        return false;
    }
    const wire::nbt::NbtCompound* stored = occupant.getCompound("entity_data");
    if (stored == nullptr) {
        return false;
    }
    wire::nbt::NbtCompound tag = *stored;
    entity::bees::stripHiveTags(tag);
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> type =
        wire::common::Identifier::parse(std::string{tag.getString("id")});
    const reg::EntityTypeEntry* entry = type.has_value() ? reg::entityTypes::of(*type) : nullptr;
    // parity: `beehive_inhabitors`, which is the bee alone.
    if (entry == nullptr || !entity::bees::isBee(*type)) {
        return false;
    }
    std::unique_ptr<ServerEntity> created =
        entity::entityNbt::decode(tag, players_->allocateEntityId(), entry->protocolId);
    if (created == nullptr) {
        return false;
    }
    created->sourceNbt = tag;
    created->noGravity = true;
    random::RandomSource random = freshRandom();
    entity::bees::BeeState& bee = entity::bees::stateOf(*created, random);
    bee.hive = hive;
    // parity: `setBeeReleaseData`: the time inside counts towards growing up and off the love.
    const std::int32_t ticks = occupant.getInt("ticks_in_hive");
    if (!tag.getBoolean("AgeLocked")) {
        if (created->age < 0) {
            created->age = std::min(0, created->age + ticks);
        } else if (created->age > 0) {
            created->age = std::max(0, created->age - ticks);
        }
    }
    created->loveTicks = std::max(0, created->loveTicks - ticks);
    if (flower.has_value() && !bee.flower.has_value() && random.nextFloat() < 0.9F) {
        bee.flower = flower;
    }
    if (release == beehives::Release::HoneyDelivered) {
        // parity: `dropOffNectar`, and a level of honey, rarely two, up to five.
        entity::bees::setHasNectar(bee, false);
        bee.cropsGrown = 0;
        const std::string levelText = propertyOf(stateId, "honey_level");
        const std::int32_t level = levelText.empty() ? beehives::kMaxHoney : std::stoi(levelText);
        if (level < beehives::kMaxHoney) {
            std::int32_t increase = random.nextInt(100) == 0 ? 2 : 1;
            if (level + increase > beehives::kMaxHoney) {
                --increase;
            }
            if (const std::optional<std::int32_t> fuller =
                    withProperty(stateId, "honey_level", std::to_string(level + increase));
                fuller.has_value()) {
                if (pending != nullptr) {
                    pending->push_back(
                        world::BlockChange{.x = hive[0], .y = hive[1], .z = hive[2], .stateId = *fuller});
                } else if (world.state().setBlockState(hive[0], hive[1], hive[2], *fuller)) {
                    const std::vector<PlayerSession*> watchers = watchersOf(world);
                    broadcaster_->announceBlock(
                        watchers, world::BlockChange{.x = hive[0], .y = hive[1], .z = hive[2], .stateId = *fuller});
                    broadcaster_->notifyAround(world, watchers, hive[0], hive[1], hive[2], std::nullopt);
                }
            }
        }
    }
    const double delta = frontBlocked ? 0.0 : 0.55 + (created->width() / 2.0);
    created->x = hive[0] + 0.5 + (delta * step[0]);
    created->y = hive[1] + 0.5 - (created->height() / 2.0);
    created->z = hive[2] + 0.5 + (delta * step[1]);
    broadcaster_->sendEffects(watchersOf(world), hive[0], hive[1], hive[2], stateId,
                              {world::blockEffect::Sound{.name = "block.beehive.exit"}});
    world::vibrations::emit(world.state(), "block_change", hive[0] + 0.5, hive[1] + 0.5, hive[2] + 0.5,
                            world.gameTime(), {.entity = created->uuid()});
    ServerEntity& added = world.state().spawnEntity(std::move(created));
    if (spawned != nullptr) {
        spawned->push_back(&added);
    }
    return true;
}

bool MobActionSystem::tickBeehive(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::vector<world::BlockChange>& pending) {
    const Pos hive{x, y, z};
    server::world::ChunkBlockEntity* entity = hiveEntity(world, hive);
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    if (entity == nullptr || !stateId.has_value() || !isHiveState(*stateId)) {
        return false;
    }
    // parity: `BeehiveBlock.updateShape`: fire arriving beside it sends every bee out at once.
    bool fireBeside = false;
    for (const auto& side : std::array<Pos, 6>{{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}}) {
        const std::optional<std::int32_t> neighbour = world.state().blockState(x + side[0], y + side[1], z + side[2]);
        const wire::common::Identifier* block =
            neighbour.has_value() ? server::registry::blockStateDefinitions::blockOf(*neighbour) : nullptr;
        fireBeside = fireBeside || (block != nullptr && block->path() == "fire");
    }
    const auto seen = hivesByFire_.find({world.name(), hive});
    const bool fireArrived = fireBeside && seen != hivesByFire_.end() && !seen->second;
    hivesByFire_[{world.name(), hive}] = fireBeside;
    if (fireArrived && releaseAll(world, hive, *stateId, entity->data(), nullptr, beehives::Release::Emergency)) {
        return true;
    }
    std::vector<std::size_t> due;
    beehives::tickOccupants(entity->data(), due);
    const std::optional<Pos> flower = beehives::flowerOf(entity->data());
    std::vector<std::size_t> released;
    for (const std::size_t index : due) {
        const wire::nbt::NbtCompound* occupant = beehives::occupant(entity->data(), index);
        if (occupant == nullptr) {
            continue;
        }
        const wire::nbt::NbtCompound* data = occupant->getCompound("entity_data");
        const beehives::Release release = data != nullptr && data->getBoolean("HasNectar")
                                              ? beehives::Release::HoneyDelivered
                                              : beehives::Release::BeeReleased;
        if (releaseOccupant(world, hive, *stateId, *occupant, release, flower, nullptr, &pending)) {
            released.push_back(index);
        }
    }
    beehives::removeOccupants(entity->data(), released);
    random::RandomSource random = freshRandom();
    if (beehives::occupantCount(entity->data()) > 0 && random.nextDouble() < 0.005) {
        broadcaster_->sendEffects(watchersOf(world), x, y, z, *stateId,
                                  {world::blockEffect::Sound{.name = "block.beehive.work"}});
    }
    // The stay counts moved on whether or not anybody left.
    return true;
}

void MobActionSystem::emptyHive(world::LoadedWorld& world, const Pos& hive, std::int32_t stateId,
                                const PlayerSession* player, beehives::Release release) {
    server::world::ChunkBlockEntity* entity = hiveEntity(world, hive);
    if (entity != nullptr && releaseAll(world, hive, stateId, entity->data(), player, release)) {
        markDirty(world, hive);
    }
}

void MobActionSystem::hiveBroken(world::LoadedWorld& world, const PlayerSession& player, const Pos& hive,
                                 std::int32_t stateId, wire::nbt::NbtCompound data, bool silkTouch) {
    if (!silkTouch) {
        static_cast<void>(releaseAll(world, hive, stateId, data, &player, beehives::Release::Emergency));
        angerBeesAround(world, hive);
    }
    // parity: `BeehiveBlock.playerDestroy` ends in `BEE_NEST_DESTROYED` with the bees still inside.
    world::AdvancementTriggers* triggers = options_.advancementTriggers;
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    if (triggers == nullptr || block == nullptr) {
        return;
    }
    world::TriggerEvent event{.numbers = {{"num_bees_inside", static_cast<double>(beehives::occupantCount(data))}},
                              .ids = {{"block", *block}}};
    if (const std::optional<world::TriggerItem> tool = world::triggerFacts::itemOf(player.inventory.heldItem());
        tool.has_value()) {
        event.items.emplace("item", *tool);
    }
    triggers->fire("bee_nest_destroyed", player.name(), player.uuid().toString(), event);
}

void MobActionSystem::hiveBlown(world::LoadedWorld& world, const Pos& hive, std::int32_t stateId,
                                wire::nbt::NbtCompound data, std::int32_t sourceEntityId) {
    const ServerEntity* source = sourceEntityId == 0 ? nullptr : world.state().entity(sourceEntityId);
    const std::string_view path = source == nullptr ? std::string_view{} : source->type().path();
    if (path == "tnt" || path == "creeper" || path == "wither_skull" || path == "wither" || path == "tnt_minecart") {
        static_cast<void>(releaseAll(world, hive, stateId, data, nullptr, beehives::Release::Emergency));
    }
    angerBeesAround(world, hive);
}

bool MobActionSystem::releaseAll(world::LoadedWorld& world, const Pos& hive, std::int32_t stateId,
                                 wire::nbt::NbtCompound& data, const PlayerSession* player, beehives::Release release) {
    const std::optional<Pos> flower = beehives::flowerOf(data);
    std::vector<ServerEntity*> spawned;
    std::vector<std::size_t> released;
    for (std::size_t index = 0; index < beehives::occupantCount(data); ++index) {
        const wire::nbt::NbtCompound* occupant = beehives::occupant(data, index);
        if (occupant != nullptr &&
            releaseOccupant(world, hive, stateId, *occupant, release, flower, &spawned, nullptr)) {
            released.push_back(index);
        }
    }
    beehives::removeOccupants(data, released);
    if (player != nullptr) {
        const bool sedated = beehives::isSmokey(world.state(), hive);
        for (ServerEntity* bee : spawned) {
            const double dx = player->x - bee->x;
            const double dy = player->y - bee->y;
            const double dz = player->z - bee->z;
            if ((dx * dx) + (dy * dy) + (dz * dz) > 16.0) {
                continue;
            }
            if (!sedated) {
                bee->targetId = player->entityId();
            } else if (bee->bee != nullptr) {
                bee->bee->stayOutOfHive = beehives::kSedatedStayOut;
            }
        }
    }
    return !spawned.empty();
}

void MobActionSystem::angerBeesAround(world::LoadedWorld& world, const Pos& hive) {
    // parity: `new AABB(pos).inflate(8, 6, 8)`, bees and players touching it.
    const double minX = hive[0] - beehives::kAngerReachXZ;
    const double maxX = hive[0] + 1.0 + beehives::kAngerReachXZ;
    const double minY = hive[1] - beehives::kAngerReachY;
    const double maxY = hive[1] + 1.0 + beehives::kAngerReachY;
    const double minZ = hive[2] - beehives::kAngerReachXZ;
    const double maxZ = hive[2] + 1.0 + beehives::kAngerReachXZ;
    std::vector<ServerEntity*> bees;
    for (ServerEntity* candidate : world.state().entities()) {
        const double half = candidate->width() / 2.0;
        if (!candidate->removed && entity::bees::isBee(candidate->type()) && candidate->x + half > minX &&
            candidate->x - half < maxX && candidate->y + candidate->height() > minY && candidate->y < maxY &&
            candidate->z + half > minZ && candidate->z - half < maxZ) {
            bees.push_back(candidate);
        }
    }
    if (bees.empty()) {
        return;
    }
    std::vector<PlayerSession*> players;
    for (PlayerSession* player : watchersOf(world)) {
        if (!player->dead && player->x + 0.3 > minX && player->x - 0.3 < maxX && player->y + 1.8 > minY &&
            player->y < maxY && player->z + 0.3 > minZ && player->z - 0.3 < maxZ) {
            players.push_back(player);
        }
    }
    if (players.empty()) {
        return;
    }
    random::RandomSource random = freshRandom();
    for (ServerEntity* bee : bees) {
        if (!bee->targetId.has_value()) {
            bee->targetId = players[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(players.size())))]
                                ->entityId();
        }
    }
}

} // namespace basalt::server::runtime
