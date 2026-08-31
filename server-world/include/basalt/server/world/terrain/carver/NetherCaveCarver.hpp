#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::carver {

/// parity: `NetherWorldCarver` with the builtin `nether_cave` configuration, applied from every
/// source chunk in the 8-chunk radius `NoiseBasedChunkGenerator.applyCarvers` walks.
void carveNether(Chunk& chunk, std::int64_t seed);

} // namespace basalt::server::world::terrain::carver
