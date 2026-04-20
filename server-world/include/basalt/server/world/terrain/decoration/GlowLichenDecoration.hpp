#pragma once

#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: `MultifaceGrowthFeature` with the `glow_lichen` configuration.
namespace glowLichenDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr);

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace glowLichenDecoration

} // namespace basalt::server::world::terrain::decoration
