#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration {

/// parity: `warm_ocean_vegetation` and the three coral features.
namespace coralDecoration {

inline constexpr std::int32_t kWarmOceanVegetationIndex = 99;

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

[[nodiscard]] BiomeVegetalDecoration vegetalDecoration();

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace coralDecoration

} // namespace basalt::server::world::terrain::decoration
