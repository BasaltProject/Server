#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration {

/// parity: `LevelChunk.postProcessGeneration` re-checks a bush once everything has landed, and
/// `updateShape` turns one that cannot stand into air. Neither rule can be decided while the features
/// run: a mushroom asks for `getRawBrightness < 13` before anything has lit the chunk, and
/// `DoublePlantBlock` asks for its other half, which a later tree can take.
namespace bushSurvival {

using NeighbourAt = std::function<const Chunk*(std::int32_t chunkX, std::int32_t chunkZ)>;

/// Removes the bushes of `chunk` that the finished chunk no longer supports.
void settle(Chunk& chunk, const NeighbourAt& neighbourAt, bool hasSkylight);

} // namespace bushSurvival

} // namespace basalt::server::world::terrain::decoration
