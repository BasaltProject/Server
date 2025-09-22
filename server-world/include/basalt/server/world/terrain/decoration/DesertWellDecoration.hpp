#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: `desert_well`, global index 2 of the Overworld `SURFACE_STRUCTURES` step.
namespace desertWellDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// The feature itself at one position: what the pass calls per origin and what `/place feature`
/// calls once. Chunk-local x and z, as everything in this package uses.
void placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t suppliedY, std::int32_t z);

} // namespace desertWellDecoration

} // namespace basalt::server::world::terrain::decoration
