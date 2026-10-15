#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::world {

/// parity: `NaturalSpawner.spawnMobsForChunkGeneration`.
namespace originalMobSpawner {

struct Options {
    /// parity: the Nether's ceiling, which sends the search down from the roof rather than up.
    bool hasCeiling = false;
    std::int64_t gameTime = 0;
    std::string difficulty = "easy";
    const SpawnEquipmentFinalizer* equipment = nullptr;
    /// Chunk generation runs off the tick thread, so the caller seeds this.
    std::function<random::RandomSource()> entityRandomFactory;
};

std::int32_t spawnInto(server::world::Chunk& chunk, const std::vector<SpawnerEntry>& entries, float creatureProbability,
                       random::RandomSource& random, const Options& options = {});

} // namespace originalMobSpawner

} // namespace basalt::server::runtime::world
