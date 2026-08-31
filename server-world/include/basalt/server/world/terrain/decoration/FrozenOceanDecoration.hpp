#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// Frozen-ocean `LOCAL_MODIFICATIONS` and `SURFACE_STRUCTURES`: packed icebergs at 1/16, blue
/// icebergs at 1/200, then 0 to 19 blue ice attempts between Y 30 and 61.
namespace frozenOceanDecoration {

void decorateLocalModifications(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

void decorateSurfaceStructures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// The vanilla placed feature names this step draws.
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace frozenOceanDecoration

} // namespace basalt::server::world::terrain::decoration
