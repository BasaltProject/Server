#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration::nether {

/// parity: the Nether's `basalt_pillar` in the local modifications step.
namespace netherLocalDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const NetherBiomeLookup& biomeSource);

/// One pillar at one position rather than scattered over the chunk.
void placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace netherLocalDecoration

} // namespace basalt::server::world::terrain::decoration::nether
