#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// Exact step-9 placement for `patch_waterlily`.
namespace waterlilyPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// The patch at one origin, which is what `/place feature` runs.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace waterlilyPatchDecoration

/// Exact step-9 placement for `patch_leaf_litter`.
namespace leafLitterPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

} // namespace leafLitterPatchDecoration

} // namespace basalt::server::world::terrain::decoration
