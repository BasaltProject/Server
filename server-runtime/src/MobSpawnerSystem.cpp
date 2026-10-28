#include <basalt/server/runtime/MobSpawnerSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>

#include <utility>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::protocol::play::Gamemode;

/// parity: `BaseSpawner.EVENT_SPAWN`, the one event a spawner broadcasts.
constexpr std::int32_t kEventSpawn = 1;

/// parity: the player's standing, crouching and sleeping heights, and its width.
constexpr double kPlayerWidth = 0.6;
constexpr double kPlayerHeight = 1.8;
constexpr double kCrouchingHeight = 1.5;
constexpr double kSleepingHeight = 0.2;

} // namespace

MobSpawnerSystem::MobSpawnerSystem(player::PlayerRegistry& players, NaturalSpawningSystem& spawning, Options options)
    : players_(&players), spawning_(&spawning), options_(std::move(options)) {}

bool MobSpawnerSystem::tick(world::LoadedWorld& world, wire::nbt::NbtCompound& data,
                            const std::array<std::int32_t, 3>& pos,
                            const std::function<void(std::int32_t eventId)>& broadcastEvent,
                            const std::function<void()>& spawnDataChanged) {
    world::BaseSpawnerState state = world::baseSpawnerData::load(data);
    if (!isNearPlayer(world, pos, state.requiredPlayerRange) ||
        !world.gameRules().get(world::gameRules::kSpawnerBlocksWork)) {
        return false;
    }
    if (world.state().gameplay().blocks.spawnerDeactivateByRedstone &&
        world::redstone::bestNeighbourSignal(world.state(), world.name(), pos[0], pos[1], pos[2]) > 0) {
        return false;
    }
    // parity: `BaseSpawner.serverTick` draws from `level.getRandom()`.
    world::LoadedWorld::RandomLease lease = world.leaseRandom();
    Turn turn{.world = world,
              .state = state,
              .pos = pos,
              .random = lease.random,
              .broadcastEvent = broadcastEvent,
              .spawnDataChanged = spawnDataChanged};
    if (state.spawnDelay == -1) {
        delay(turn);
    }
    if (state.spawnDelay > 0) {
        --state.spawnDelay;
    } else {
        const world::SpawnData next = nextSpawnData(turn);
        bool spawned = false;
        for (std::int32_t count = 0; count < state.spawnCount; ++count) {
            const Attempt result = attempt(turn, next);
            // parity: an unknown type, an entity that would not load, a full cage or a refused add.
            if (result == Attempt::Abort) {
                delay(turn);
                spawned = false;
                break;
            }
            spawned = spawned || result == Attempt::Spawned;
        }
        if (spawned) {
            delay(turn);
        }
    }
    // parity: `BaseSpawner.save`.
    if (turn.dataChanged) {
        world::baseSpawnerData::save(state, data);
    } else {
        data.putShort("Delay", static_cast<std::int16_t>(state.spawnDelay));
    }
    return true;
}

bool MobSpawnerSystem::isNearPlayer(const world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos,
                                    std::int32_t range) const {
    const double x = pos[0] + 0.5;
    const double y = pos[1] + 0.5;
    const double z = pos[2] + 0.5;
    const auto reach = static_cast<double>(range);
    for (const PlayerSession* player : playersIn(world)) {
        // parity: `NO_SPECTATORS` and `LIVING_ENTITY_STILL_ALIVE`.
        if (player->gamemode == Gamemode::Spectator || player->dead || player->health() <= 0.0F) {
            continue;
        }
        const double dx = player->x - x;
        const double dy = player->y - y;
        const double dz = player->z - z;
        if (reach < 0.0 || (dx * dx) + (dy * dy) + (dz * dz) < reach * reach) {
            return true;
        }
    }
    return false;
}

void MobSpawnerSystem::delay(Turn& turn) const {
    world::BaseSpawnerState& state = turn.state;
    // parity: Java's int arithmetic, which a bound wrapped past zero would have thrown on.
    const auto bound = static_cast<std::int32_t>(static_cast<std::uint32_t>(state.maxSpawnDelay) -
                                                 static_cast<std::uint32_t>(state.minSpawnDelay));
    if (state.maxSpawnDelay <= state.minSpawnDelay || bound <= 0) {
        state.spawnDelay = state.minSpawnDelay;
    } else {
        state.spawnDelay = state.minSpawnDelay + turn.random.nextInt(bound);
    }
    if (const world::SpawnData* picked = world::baseSpawnerData::pick(state.spawnPotentials, turn.random);
        picked != nullptr) {
        state.nextSpawnData = *picked;
        turn.dataChanged = true;
        if (turn.spawnDataChanged) {
            turn.spawnDataChanged();
        }
    }
    if (turn.broadcastEvent) {
        turn.broadcastEvent(kEventSpawn);
    }
}

const world::SpawnData& MobSpawnerSystem::nextSpawnData(Turn& turn) const {
    world::BaseSpawnerState& state = turn.state;
    if (state.nextSpawnData.has_value()) {
        return *state.nextSpawnData;
    }
    const world::SpawnData* picked = world::baseSpawnerData::pick(state.spawnPotentials, turn.random);
    state.nextSpawnData = picked != nullptr ? *picked : world::SpawnData{};
    turn.dataChanged = true;
    if (turn.spawnDataChanged) {
        turn.spawnDataChanged();
    }
    return *state.nextSpawnData;
}

std::vector<PlayerSession*> MobSpawnerSystem::playersIn(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> found;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            found.push_back(player);
        }
    }
    return found;
}

std::vector<entity::Aabb> MobSpawnerSystem::playerBoxes(const world::LoadedWorld& world) const {
    std::vector<entity::Aabb> boxes;
    for (const PlayerSession* player : playersIn(world)) {
        if (player->gamemode == Gamemode::Spectator) {
            continue;
        }
        const double height = player->sleepingBed.has_value() ? kSleepingHeight
                              : player->sneaking              ? kCrouchingHeight
                                                              : kPlayerHeight;
        boxes.push_back(entity::Aabb::of(player->x, player->y, player->z, kPlayerWidth, height));
    }
    return boxes;
}

} // namespace basalt::server::runtime
