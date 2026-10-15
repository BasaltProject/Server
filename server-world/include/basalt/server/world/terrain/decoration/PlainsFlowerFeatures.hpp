#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

namespace plainsDecoration {

void placeFlowerPlainAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::int32_t flowerStateId(random::RandomSource& random, const DecorationPos& origin);

} // namespace plainsDecoration

namespace forestFloorDecoration {

/// parity: `wildflowers_birch_forest`, three independent rarity-1/2 patch attempts.
[[nodiscard]] const BiomeVegetalDecoration& birchWildflowers();

/// parity: `wildflowers_meadow`.
[[nodiscard]] const BiomeVegetalDecoration& meadowWildflowers();

/// parity: `patch_leaf_litter`, two 32-candidate patches restricted to grass block.
[[nodiscard]] const BiomeVegetalDecoration& leafLitter();

void placeWildflowersAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::int32_t count);

[[nodiscard]] std::int32_t flowerBedStateId(std::int32_t defaultStateId, std::int32_t choice);

} // namespace forestFloorDecoration

} // namespace basalt::server::world::terrain::decoration
