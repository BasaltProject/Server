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

/// parity: `warm_ocean_vegetation` and the three coral features, including the waterlogged coral
/// states, the wall-fan facing and the sea pickle count.
namespace coralDecoration {

inline constexpr std::int32_t kWarmOceanVegetationIndex = 96;

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// The same step as a biome's own vegetal entry, which runs on an already seeded random.
[[nodiscard]] BiomeVegetalDecoration vegetalDecoration();

/// The vanilla placed feature names this step draws.
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One coral growth at one position: vanilla picks between the three shapes, and so does this.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace coralDecoration

} // namespace basalt::server::world::terrain::decoration
