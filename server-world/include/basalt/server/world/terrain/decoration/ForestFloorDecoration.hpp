#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// The specialized birch and dark forest floor plants.
namespace forestFloorDecoration {

/// parity: `wildflowers_birch_forest`, three independent rarity-1/2 patch attempts.
[[nodiscard]] const BiomeVegetalDecoration& birchWildflowers();

/// parity: `wildflowers_meadow`, biome-noise outer density and eight candidates per patch.
[[nodiscard]] const BiomeVegetalDecoration& meadowWildflowers();

/// parity: `patch_leaf_litter`, two 32-candidate patches restricted to grass block.
[[nodiscard]] const BiomeVegetalDecoration& leafLitter();

/// One wildflower patch at one origin, which is what `/place feature` runs.
void placeWildflowersAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::int32_t count);

/// Both flower beds and leaf litter pack facing and amount into one state offset.
[[nodiscard]] std::int32_t flowerBedStateId(std::int32_t defaultStateId, std::int32_t choice);

} // namespace forestFloorDecoration

} // namespace basalt::server::world::terrain::decoration
