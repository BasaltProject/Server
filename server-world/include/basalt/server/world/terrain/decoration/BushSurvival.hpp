#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration {

/// parity: `LevelChunk.postProcessGeneration` re-checks a bush once everything has landed.
namespace bushSurvival {

using NeighbourAt = std::function<const Chunk*(std::int32_t chunkX, std::int32_t chunkZ)>;

void settle(Chunk& chunk, const NeighbourAt& neighbourAt, bool hasSkylight);

} // namespace bushSurvival

} // namespace basalt::server::world::terrain::decoration
