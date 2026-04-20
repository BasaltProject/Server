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

namespace frozenOceanDecoration {

void decorateLocalModifications(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

void decorateSurfaceStructures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace frozenOceanDecoration

} // namespace basalt::server::world::terrain::decoration
