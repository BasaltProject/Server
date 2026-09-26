#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <array>
#include <cmath>
#include <deque>
#include <set>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
using wire::protocol::play::SoundSource;
namespace creakings = entity::creakings;
namespace defs = server::registry::blockStateDefinitions;
using Pos = std::array<std::int32_t, 3>;

/// parity: `CreakingHeartBlockEntity`'s ranges.
constexpr double kPlayerRange = 32.0;
constexpr double kTooFar = 34.0;
constexpr std::int32_t kSpawnAttempts = 5;
constexpr std::int32_t kSpawnRangeXZ = 16;
constexpr std::int32_t kSpawnRangeY = 8;
constexpr std::int64_t kGraceTicks = 30;
/// parity: `Creaking.CREAKING_ORANGE` and `CREAKING_GRAY`.
constexpr std::int32_t kOrange = 16545810;
constexpr std::int32_t kGray = 6250335;
/// parity: `EntityType.CREAKING`'s box.
constexpr double kCreakingWidth = 0.9;
constexpr double kCreakingHeight = 2.7;

/// parity: `Direction.values()`: down, up, north, south, west, east.
constexpr std::array<Pos, 6> kDirections{Pos{0, -1, 0}, Pos{0, 1, 0},  Pos{0, 0, -1},
                                         Pos{0, 0, 1},  Pos{-1, 0, 0}, Pos{1, 0, 0}};
constexpr std::array<std::string_view, 6> kFaces{"down", "up", "north", "south", "west", "east"};

[[nodiscard]] std::string_view pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = defs::blockOf(stateId);
    return block == nullptr ? std::string_view{} : block->path();
}

[[nodiscard]] std::string property(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = defs::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

/// parity: `Util.shuffledCopy(Direction.values(), random)`.
[[nodiscard]] std::array<std::size_t, 6> shuffledDirections(random::RandomSource& random) {
    std::array<std::size_t, 6> order{0, 1, 2, 3, 4, 5};
    for (std::int32_t index = 6; index > 1; --index) {
        const auto swapTo = static_cast<std::size_t>(random.nextInt(index));
        std::swap(order[static_cast<std::size_t>(index - 1)], order[swapTo]);
    }
    return order;
}

[[nodiscard]] server::world::ChunkBlockEntity* heartEntity(world::LoadedWorld& world, const Pos& at) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
    server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(at[0] & 15, at[1], at[2] & 15);
    return entity != nullptr && entity->type().path() == "creaking_heart" ? entity : nullptr;
}

[[nodiscard]] double distanceToHeart(const ServerEntity& creaking, const Pos& at) {
    const double dx = creaking.x - (at[0] + 0.5);
    const double dy = creaking.y - at[1];
    const double dz = creaking.z - (at[2] + 0.5);
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

} // namespace

std::optional<bool> MobActionSystem::heartClaims(world::LoadedWorld& world, const Pos& home,
                                                 const ServerEntity& creaking) {
    if (world.state().chunkIfLoaded(home[0] >> 4, home[2] >> 4) == nullptr) {
        return false;
    }
    const server::world::ChunkBlockEntity* heart = heartEntity(world, home);
    if (heart == nullptr) {
        return std::nullopt;
    }
    return creakings::heartCreaking(heart->data()) == creaking.uuid();
}

ServerEntity* MobActionSystem::heartProtector(world::LoadedWorld& world, const Pos& at, creakings::HeartState& heart) {
    server::world::ChunkBlockEntity* entity = heartEntity(world, at);
    if (entity == nullptr) {
        return nullptr;
    }
    const std::optional<wire::common::Uuid> bound = creakings::heartCreaking(entity->data());
    if (!bound.has_value()) {
        return nullptr;
    }
    for (ServerEntity* candidate : world.state().entities()) {
        if (!candidate->removed && candidate->uuid() == *bound && creakings::isCreaking(candidate->type())) {
            return candidate;
        }
    }
    // parity: a heart whose creaking has not turned up after its grace period lets it go.
    if (heart.ticksExisted >= kGraceTicks) {
        creakings::setHeartCreaking(entity->data(), std::nullopt);
    }
    return nullptr;
}

void MobActionSystem::heartParticles(world::LoadedWorld& world, const Pos& at, const ServerEntity& creaking,
                                     std::int32_t count, bool towardsCreaking, random::RandomSource& random) {
    // parity: `emitParticles`: trails between a point in the creaking's box and one in the heart.
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    const double halfWidth = creaking.width() / 2.0;
    for (std::int32_t index = 0; index < count; ++index) {
        std::array<double, 3> source{creaking.x - halfWidth + (random.nextDouble() * creaking.width()),
                                     creaking.y + (random.nextDouble() * creaking.height()),
                                     creaking.z - halfWidth + (random.nextDouble() * creaking.width())};
        std::array<double, 3> destination{at[0] + random.nextDouble(), at[1] + random.nextDouble(),
                                          at[2] + random.nextDouble()};
        if (towardsCreaking) {
            std::swap(source, destination);
        }
        const wire::protocol::play::LevelParticlesPacket packet{
            .particleTypeId = ExplosionSystem::particleId("trail"),
            .x = source[0],
            .y = source[1],
            .z = source[2],
            .count = 1,
            .overrideLimiter = true,
            .alwaysShow = true,
            .options = wire::protocol::play::ParticleOptions{
                wire::protocol::play::ParticleOptions::Trail{.targetX = destination[0],
                                                             .targetY = destination[1],
                                                             .targetZ = destination[2],
                                                             .color = towardsCreaking ? kOrange : kGray,
                                                             .duration = random.nextInt(40) + 10}}};
        for (const PlayerSession* watcher : watchers) {
            player::showParticleFor(*watcher, "minecraft:trail", packet);
        }
    }
}

void MobActionSystem::creakingHeartHurt(world::LoadedWorld& world, const Pos& at) {
    creakings::HeartState& heart = creakingHearts_[{world.name(), at}];
    ServerEntity* creaking = heartProtector(world, at, heart);
    if (creaking == nullptr || heart.emitter > 0) {
        return;
    }
    random::RandomSource random = freshRandom();
    heartParticles(world, at, *creaking, 20, false, random);
    const std::optional<std::int32_t> stateId = world.state().blockState(at[0], at[1], at[2]);
    if (stateId.has_value() && property(*stateId, "creaking_heart_state") == "awake") {
        const std::int32_t clumps = random.nextIntBetweenInclusive(2, 3);
        for (std::int32_t clump = 0; clump < clumps; ++clump) {
            if (const std::optional<Pos> placed = spreadResin(world, at, random); placed.has_value()) {
                broadcaster_->sendEffects(
                    watchersOf(world), (*placed)[0], (*placed)[1], (*placed)[2],
                    world.state().blockState((*placed)[0], (*placed)[1], (*placed)[2]).value_or(0),
                    {world::blockEffect::Sound{
                        .name = "block.resin.place", .source = SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F}});
                world::vibrations::emit(world.state(), "block_place", (*placed)[0] + 0.5, (*placed)[1] + 0.5,
                                        (*placed)[2] + 0.5, world.gameTime(), {});
            }
        }
    }
    heart.emitter = 100;
    heart.emitterTarget = std::array<double, 3>{creaking->x, creaking->y + (creaking->height() / 2.0), creaking->z};
}

std::optional<Pos> MobActionSystem::spreadResin(world::LoadedWorld& world, const Pos& from,
                                                random::RandomSource& random) {
    const auto isLog = [this, &world](const Pos& at) {
        const std::optional<std::int32_t> state = world.state().blockState(at[0], at[1], at[2]);
        const wire::common::Identifier* block = state.has_value() ? defs::blockOf(*state) : nullptr;
        return block != nullptr && blockTagged("minecraft:pale_oak_logs", *block).value_or(false);
    };
    // parity: `BlockPos.breadthFirstTraversal(pos, 2, 64, ...)` through pale oak logs.
    std::deque<std::pair<Pos, std::int32_t>> nodes{{from, 0}};
    std::set<Pos> visited;
    std::int32_t count = 0;
    while (!nodes.empty()) {
        const auto [at, depth] = nodes.front();
        nodes.pop_front();
        if (!visited.insert(at).second) {
            continue;
        }
        if (isLog(at)) {
            for (const std::size_t direction : shuffledDirections(random)) {
                const Pos next{at[0] + kDirections[direction][0], at[1] + kDirections[direction][1],
                               at[2] + kDirections[direction][2]};
                std::optional<std::int32_t> neighbour = world.state().blockState(next[0], next[1], next[2]);
                if (!neighbour.has_value()) {
                    continue;
                }
                const std::string_view opposite = kFaces[direction ^ 1U];
                const wire::common::Identifier resin = wire::common::Identifier::vanilla("resin_clump");
                std::optional<std::int32_t> candidate;
                if (pathOf(*neighbour) == "air" || pathOf(*neighbour) == "cave_air" ||
                    pathOf(*neighbour) == "void_air") {
                    candidate = server::registry::blocks::defaultStateIdOf(resin);
                } else if (pathOf(*neighbour) == "water" && property(*neighbour, "level") == "0") {
                    candidate = defs::stateIdOf(resin, {{"waterlogged", "true"}},
                                                server::registry::blocks::defaultStateIdOf(resin));
                } else if (pathOf(*neighbour) == "resin_clump") {
                    candidate = *neighbour;
                }
                if (!candidate.has_value() || property(*candidate, std::string{opposite}) == "true") {
                    continue;
                }
                const std::optional<std::int32_t> placed =
                    defs::stateIdOf(resin, {{std::string{opposite}, "true"}}, *candidate);
                if (placed.has_value() && world.state().setBlockState(next[0], next[1], next[2], *placed)) {
                    const std::vector<PlayerSession*> watchers = watchersOf(world);
                    broadcaster_->announceBlock(
                        watchers, world::BlockChange{.x = next[0], .y = next[1], .z = next[2], .stateId = *placed});
                    broadcaster_->notifyAround(world, watchers, next[0], next[1], next[2], neighbour);
                    return next;
                }
            }
        }
        if (++count >= 64) {
            return std::nullopt;
        }
        if (depth < 2) {
            for (const std::size_t direction : shuffledDirections(random)) {
                const Pos next{at[0] + kDirections[direction][0], at[1] + kDirections[direction][1],
                               at[2] + kDirections[direction][2]};
                if (isLog(next)) {
                    nodes.emplace_back(next, depth + 1);
                }
            }
        }
    }
    return std::nullopt;
}

void MobActionSystem::removeCreakingProtector(world::LoadedWorld& world, const Pos& at, const PlayerSession* breaker) {
    creakings::HeartState& heart = creakingHearts_[{world.name(), at}];
    ServerEntity* creaking = heartProtector(world, at, heart);
    if (creaking != nullptr) {
        creakings::CreakingState& state = creakings::stateOf(*creaking);
        if (breaker == nullptr) {
            tearDownCreaking(world, *creaking);
        } else {
            // parity: `creakingDeathEffects`: it dies at the breaker's hand and twitches for its last
            // forty-five ticks before it comes apart.
            creaking->lastBlame.causingId = breaker->entityId();
            entity::entityCombat::rememberPlayer(*creaking, breaker->uuid());
            entityEvent(*creaking, wire::protocol::play::EntityEventPacket::kLivingDeath);
            makeSound(world, *creaking, "entity.creaking.twitch", 1.0F);
            state.tearingDown = true;
            creaking->health = 0.0;
            creaking->killed = true;
            creaking->dying = true;
        }
    }
    if (server::world::ChunkBlockEntity* entity = heartEntity(world, at); entity != nullptr) {
        creakings::setHeartCreaking(entity->data(), std::nullopt);
    }
}

void MobActionSystem::creakingHeartBroken(world::LoadedWorld& world, const PlayerSession& player, const Pos& at) {
    removeCreakingProtector(world, at, &player);
    creakingHearts_.erase({world.name(), at});
}

bool MobActionSystem::tickCreakingHeart(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                        std::vector<world::BlockChange>& pending) {
    const Pos at{x, y, z};
    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    // parity: `CreakingHeartBlock.getTicker`, nothing for an uprooted heart.
    if (!stateId.has_value() || property(*stateId, "creaking_heart_state") == "uprooted") {
        return false;
    }
    creakings::HeartState& heart = creakingHearts_[{world.name(), at}];
    ++heart.ticksExisted;
    ServerEntity* creaking = heartProtector(world, at, heart);
    // parity: the analogue signal, cached, and a comparator told when it moves.
    const std::int32_t signal = creaking == nullptr ? 0 : creakings::signalFor(distanceToHeart(*creaking, at));
    if (signal != heart.outputSignal) {
        heart.outputSignal = signal;
        world::redstoneComponents::scheduleComparatorsAround(world.state(), world.name(), x, y, z,
                                                             world::comparatorReading::of(world.state()));
    }
    random::RandomSource random = freshRandom();
    if (heart.emitter > 0) {
        if (creaking != nullptr && heart.emitter > 50) {
            heartParticles(world, at, *creaking, 1, true, random);
            heartParticles(world, at, *creaking, 1, false, random);
        }
        if (heart.emitter % 10 == 0 && heart.emitterTarget.has_value()) {
            if (creaking != nullptr) {
                heart.emitterTarget =
                    std::array<double, 3>{creaking->x, creaking->y + (creaking->height() / 2.0), creaking->z};
            }
            // parity: the hurt sound walks from the creaking to the heart over the five seconds.
            const float progress = 0.2F + (0.8F * static_cast<float>(100 - heart.emitter) / 100.0F);
            std::array<double, 3> sound{};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                const double centre = at[axis] + 0.5;
                sound[axis] = ((centre - (*heart.emitterTarget)[axis]) * static_cast<double>(progress)) +
                              (*heart.emitterTarget)[axis];
            }
            const float volume = (static_cast<float>(heart.emitter) / 2.0F / 100.0F) + 0.5F;
            broadcaster_->sendEffects(watchersOf(world), static_cast<std::int32_t>(std::floor(sound[0])),
                                      static_cast<std::int32_t>(std::floor(sound[1])),
                                      static_cast<std::int32_t>(std::floor(sound[2])), *stateId,
                                      {world::blockEffect::Sound{.name = "block.creaking_heart.hurt",
                                                                 .source = SoundSource::Blocks,
                                                                 .volume = volume,
                                                                 .pitch = 1.0F}});
        }
        --heart.emitter;
    }
    if (heart.ticker-- >= 0) {
        return false;
    }
    heart.ticker = random.nextInt(5) + 20;
    const bool active = world::scheduledBlocks::creakingIsActive(
        world.clock().totalTicks(), config::dimensionFacts(world.declaration().dimension).hasDayTimeline);
    // parity: `updateCreakingState`: uprooted without its logs and its creaking, else awake by night.
    const server::world::ChunkBlockEntity* entity = heartEntity(world, at);
    const bool bound = entity != nullptr && creakings::heartCreaking(entity->data()).has_value();
    const std::string axis = property(*stateId, "axis");
    bool logs = true;
    for (const std::int32_t sign : {-1, 1}) {
        const Pos next{x + (axis == "x" ? sign : 0), y + (axis == "y" ? sign : 0), z + (axis == "z" ? sign : 0)};
        const std::optional<std::int32_t> neighbour = world.state().blockState(next[0], next[1], next[2]);
        const wire::common::Identifier* block = neighbour.has_value() ? defs::blockOf(*neighbour) : nullptr;
        logs = logs && block != nullptr && blockTagged("minecraft:pale_oak_logs", *block).value_or(false) &&
               property(*neighbour, "axis") == axis;
    }
    const std::string_view next = !logs && !bound ? "uprooted" : active ? "awake" : "dormant";
    bool changed = false;
    if (property(*stateId, "creaking_heart_state") != next) {
        if (const std::optional<std::int32_t> updated =
                defs::stateIdOf(wire::common::Identifier::vanilla("creaking_heart"),
                                {{"creaking_heart_state", std::string{next}}}, *stateId);
            updated.has_value()) {
            pending.push_back(world::BlockChange{.x = x, .y = y, .z = z, .stateId = *updated});
            changed = true;
        }
        if (next == "uprooted") {
            return changed;
        }
    }
    if (!bound) {
        if (next != "awake" || !world.gameRules().get(world::gameRules::kSpawnMonsters) ||
            world.difficulty() == "peaceful") {
            return changed;
        }
        bool nearby = false;
        for (const PlayerSession* player : watchersOf(world)) {
            const double dx = player->x - x;
            const double dy = player->y - y;
            const double dz = player->z - z;
            nearby = nearby || (player->gamemode != Gamemode::Spectator && !player->dead &&
                                (dx * dx) + (dy * dy) + (dz * dz) < kPlayerRange * kPlayerRange);
        }
        if (nearby) {
            if (ServerEntity* spawned = spawnProtector(world, at, random); spawned != nullptr) {
                if (server::world::ChunkBlockEntity* stored = heartEntity(world, at); stored != nullptr) {
                    creakings::setHeartCreaking(stored->data(), spawned->uuid());
                }
                makeSound(world, *spawned, "entity.creaking.spawn", 1.0F);
                broadcaster_->sendEffects(watchersOf(world), x, y, z, *stateId,
                                          {world::blockEffect::Sound{.name = "block.creaking_heart.spawn",
                                                                     .source = SoundSource::Blocks,
                                                                     .volume = 1.0F,
                                                                     .pitch = 1.0F}});
                changed = true;
            }
        }
        return changed;
    }
    if (creaking != nullptr) {
        const creakings::CreakingState& state = creakings::stateOf(*creaking);
        if ((!active && !creaking->persistent) || distanceToHeart(*creaking, at) > kTooFar ||
            state.playerStuckCounter > creakings::kStuckLimit) {
            removeCreakingProtector(world, at, nullptr);
            changed = true;
        }
    }
    return changed;
}

ServerEntity* MobActionSystem::spawnProtector(world::LoadedWorld& world, const Pos& at, random::RandomSource& random) {
    if (spawning_ == nullptr) {
        return nullptr;
    }
    const entity::entityCollision::BlockAt blockAt = [&world](std::int32_t bx, std::int32_t by, std::int32_t bz) {
        return world.state().blockState(bx, by, bz);
    };
    // parity: `SpawnUtil.trySpawnMob(..., 5, 16, 8, ON_TOP_OF_COLLIDER_NO_LEAVES, true)`.
    for (std::int32_t attempt = 0; attempt < kSpawnAttempts; ++attempt) {
        const std::int32_t sx = at[0] + random.nextIntBetweenInclusive(-kSpawnRangeXZ, kSpawnRangeXZ);
        const std::int32_t sz = at[2] + random.nextIntBetweenInclusive(-kSpawnRangeXZ, kSpawnRangeXZ);
        std::int32_t sy = at[1] + kSpawnRangeY;
        std::optional<std::int32_t> above = world.state().blockState(sx, sy, sz);
        std::optional<std::int32_t> found;
        for (std::int32_t step = kSpawnRangeY; step >= -kSpawnRangeY && above.has_value(); --step) {
            --sy;
            const std::optional<std::int32_t> current = world.state().blockState(sx, sy, sz);
            if (!current.has_value()) {
                break;
            }
            const wire::common::Identifier* block = defs::blockOf(*current);
            if (server::registry::blockShapes::isEmptyShape(*above) && block != nullptr &&
                !blockTagged("minecraft:leaves", *block).value_or(false) &&
                server::registry::blockShapes::isCollisionFaceFull(*current, server::registry::BlockFace::Up)) {
                found = sy + 1;
                break;
            }
            above = current;
        }
        if (!found.has_value() ||
            entity::entityCollision::collides(
                blockAt, entity::Aabb::of(sx + 0.5, *found, sz + 0.5, kCreakingWidth, kCreakingHeight))) {
            continue;
        }
        ServerEntity* spawned =
            spawning_->spawnMob(world, wire::common::Identifier::vanilla("creaking"), sx + 0.5, *found, sz + 0.5, 0.0F,
                                "spawner", [&at](ServerEntity& mob) { creakings::bindTo(mob, at); });
        if (spawned == nullptr) {
            continue;
        }
        // parity: `playAmbientSound`, the place event and the puff it comes out of.
        makeSound(world, *spawned, "entity.creaking.ambient", 1.0F);
        world::vibrations::emit(world.state(), "entity_place", spawned->x, spawned->y, spawned->z, world.gameTime(),
                                {.entity = spawned->uuid()});
        entityEvent(*spawned, creakings::kEventSpawnPoof);
        return spawned;
    }
    return nullptr;
}

} // namespace basalt::server::runtime
