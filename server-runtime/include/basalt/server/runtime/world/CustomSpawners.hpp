#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {

class LoadedWorld;

/// Minecraft 26.2's Overworld custom spawner for insomnia phantoms.
///
/// parity: `PhantomSpawner`. Its own clock, its own difficulty roll, and the player's own
/// `time_since_rest`, which is why phantoms are a punishment for not sleeping rather than weather.
class PhantomSpawner {
public:
    using Spawn = std::function<void(const SpawnRequest&)>;

    void tick(LoadedWorld& world, bool spawnEnemies, const std::vector<player::PlayerSession*>& players,
              random::RandomSource& random, const Spawn& spawn);

private:
    std::int32_t nextTick_ = 0;
};

/// One cat the spawner asked for, with the world conditions its variant is chosen from.
struct CatSpawnRequest {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool persistent = false;
    CatSpawnContext context;
    std::optional<wire::common::Identifier> biome;
};

/// Minecraft 26.2's Overworld custom spawner for village and swamp-hut cats.
class CatSpawner {
public:
    using Spawn = std::function<void(const CatSpawnRequest&)>;

    /// Structure-piece membership; false until the Overworld generator supplies swamp huts.
    using InStructureAt = std::function<bool(LoadedWorld&, std::int32_t, std::int32_t, std::int32_t)>;

    explicit CatSpawner(InStructureAt catsSpawnInStructureAt = {});

    void tick(LoadedWorld& world, const std::vector<player::PlayerSession*>& players, random::RandomSource& random,
              const Spawn& spawn);

private:
    InStructureAt inStructureAt_;
    std::int32_t nextTick_ = 0;
};

/// One member of a patrol, and whether it is the captain carrying the banner.
struct PatrolSpawnRequest {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool leader = false;
    std::optional<wire::common::Identifier> biome;
};

/// Minecraft 26.2's Overworld custom spawner for pillager patrols.
///
/// parity: `PatrolSpawner`. A patrol appears away from a village, in the dark, after the fifth day,
/// and the whole group is abandoned if its captain has nowhere to stand.
class PatrolSpawner {
public:
    /// Answers whether the member was really spawned, which is what decides if the rest follow.
    using Spawn = std::function<bool(const PatrolSpawnRequest&)>;

    void tick(LoadedWorld& world, bool spawnEnemies, const std::vector<player::PlayerSession*>& players,
              random::RandomSource& random, const Spawn& spawn);

private:
    std::int32_t nextTick_ = 0;
};

/// Where the trader appeared and what it walks back to. parity: the trader remembers a wandering
/// origin, which is what stops it walking off the map.
struct WanderingTraderSpawnRequest {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::int32_t referenceX = 0;
    std::int32_t referenceY = 0;
    std::int32_t referenceZ = 0;
};

struct TraderLlamaSpawnRequest {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// What the spawner keeps between restarts, which is a real save field in vanilla.
struct WanderingTraderSpawnState {
    std::int32_t spawnDelay = 24000;
    std::int32_t spawnChance = 25;
};

/// Minecraft 26.2's persisted periodic wandering-trader spawner.
class WanderingTraderSpawner {
public:
    /// Answers the trader's entity id, or nothing when the caller refused to make one: the llamas
    /// need something to follow, so no trader means no llamas either.
    using SpawnTrader = std::function<std::optional<std::int32_t>(const WanderingTraderSpawnRequest&)>;
    using SpawnLlama = std::function<void(const TraderLlamaSpawnRequest&, std::int32_t traderId)>;

    /// `random` is the spawner's own stream, separate from the level's: vanilla gives this one its
    /// own so the trader's arrival does not shift what else spawns that tick.
    explicit WanderingTraderSpawner(WanderingTraderSpawnState state = {},
                                    random::RandomSource random = random::RandomSource::legacy(0));

    void tick(LoadedWorld& world, const std::vector<player::PlayerSession*>& players, random::RandomSource& levelRandom,
              const SpawnTrader& spawnTrader, const SpawnLlama& spawnLlama);

    [[nodiscard]] WanderingTraderSpawnState savedState() const noexcept { return state_; }

private:
    [[nodiscard]] bool spawn(LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                             random::RandomSource& levelRandom, const SpawnTrader& spawnTrader,
                             const SpawnLlama& spawnLlama);

    WanderingTraderSpawnState state_;
    random::RandomSource random_;
    std::int32_t tickDelay_ = 1200;
};

} // namespace basalt::server::runtime::world
