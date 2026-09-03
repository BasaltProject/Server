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

} // namespace basalt::server::runtime::world
