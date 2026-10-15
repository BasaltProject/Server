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

/// parity: `PhantomSpawner`.
class PhantomSpawner {
public:
    using Spawn = std::function<void(const SpawnRequest&)>;

    void tick(LoadedWorld& world, bool spawnEnemies, const std::vector<player::PlayerSession*>& players,
              random::RandomSource& random, const Spawn& spawn);

private:
    std::int32_t nextTick_ = 0;
};

struct CatSpawnRequest {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool persistent = false;
    CatSpawnContext context;
    std::optional<wire::common::Identifier> biome;
};

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

struct PatrolSpawnRequest {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool leader = false;
    std::optional<wire::common::Identifier> biome;
};

/// parity: `PatrolSpawner`.
class PatrolSpawner {
public:
    /// Answers whether the member was really spawned, which is what decides if the rest follow.
    using Spawn = std::function<bool(const PatrolSpawnRequest&)>;

    void tick(LoadedWorld& world, bool spawnEnemies, const std::vector<player::PlayerSession*>& players,
              random::RandomSource& random, const Spawn& spawn);

private:
    std::int32_t nextTick_ = 0;
};

/// parity: the trader remembers a wandering origin, which stops it walking off the map.
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

struct WanderingTraderSpawnState {
    std::int32_t spawnDelay = 24000;
    std::int32_t spawnChance = 25;
};

class WanderingTraderSpawner {
public:
    using SpawnTrader = std::function<std::optional<std::int32_t>(const WanderingTraderSpawnRequest&)>;
    using SpawnLlama = std::function<void(const TraderLlamaSpawnRequest&, std::int32_t traderId)>;

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

struct VillageSiegeSpawnRequest {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// parity: `VillageSiege`.
class VillageSiegeSpawner {
public:
    using Spawn = std::function<void(const VillageSiegeSpawnRequest&)>;

    void tick(LoadedWorld& world, bool spawnEnemies, const std::vector<player::PlayerSession*>& players,
              random::RandomSource& random, const Spawn& spawn);

private:
    /// True once a qualifying player is found, even if all ten centre probes fail.
    [[nodiscard]] bool trySetUpSiege(LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                                     random::RandomSource& random);

    bool hasSetUpSiege_ = false;
    bool siegeTonight_ = false;
    std::int32_t zombiesToSpawn_ = 0;
    std::int32_t nextSpawnTime_ = 0;
    std::int32_t spawnX_ = 0;
    std::int32_t spawnY_ = 0;
    std::int32_t spawnZ_ = 0;
};

} // namespace basalt::server::runtime::world
