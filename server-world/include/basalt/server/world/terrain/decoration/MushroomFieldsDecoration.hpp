#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

namespace mushroomFieldsDecoration {

inline constexpr std::int32_t kMushroomIslandVegetationIndex = 82;

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

[[nodiscard]] BiomeVegetalDecoration vegetalDecoration();

[[nodiscard]] BiomeVegetalDecoration hugeOnly();

/// parity: `mushroom_island_vegetation`, one huge mushroom at one position.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace mushroomFieldsDecoration

} // namespace basalt::server::world::terrain::decoration
