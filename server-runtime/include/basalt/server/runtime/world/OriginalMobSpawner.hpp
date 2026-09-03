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

/// Passive packs created by the chunk-generation spawn status.
///
/// parity: `NaturalSpawner.spawnMobsForChunkGeneration`: weighted creature packs, the biome's own
/// probability, four position attempts per member and a position clamped inside the generated chunk.
namespace originalMobSpawner {

/// What one pass needs besides the chunk and its spawn list.
struct Options {
    /// parity: the Nether's ceiling, which sends the search down from the roof rather than up.
    bool hasCeiling = false;
    std::int64_t gameTime = 0;
    std::string difficulty = "easy";
    const SpawnEquipmentFinalizer* equipment = nullptr;
    /// A fresh per-entity stream. Chunk generation runs off the tick thread, so this is the caller's
    /// to seed rather than something taken from the world.
    std::function<random::RandomSource()> entityRandomFactory;
};

/// Adds what it decided straight to the chunk, and answers how many mobs that was.
std::int32_t spawnInto(server::world::Chunk& chunk, const std::vector<SpawnerEntry>& entries, float creatureProbability,
                       random::RandomSource& random, const Options& options = {});

} // namespace originalMobSpawner

} // namespace basalt::server::runtime::world
