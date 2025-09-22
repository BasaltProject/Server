#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: the `patch_tall_grass` and `patch_tall_grass_2` placed features.
namespace tallGrassDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// One spec on its own, addressed by its global feature index.
void decorateFeature(std::int32_t index, Chunk& chunk, std::int64_t seed,
                     const surface::SurfaceRuleEnvironment& environment,
                     ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// The patch at one origin, which is what `/place feature` runs for `patch_tall_grass`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace tallGrassDecoration

} // namespace basalt::server::world::terrain::decoration
