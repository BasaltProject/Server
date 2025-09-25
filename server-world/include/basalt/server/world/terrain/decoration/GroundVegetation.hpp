#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration {

/// The shared `flowerDefault` patch behind `flower_default` and `flower_warm`, which differ only in
/// their rarity filter. parity: the weighted provider is poppy 2, dandelion 1, so `nextInt(3) < 2`
/// picks poppy.
namespace defaultVegetationDecoration {

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// The patch at one origin, which is what `/place feature` runs.
void placeDefaultPatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace defaultVegetationDecoration

/// The 26.2 `dry_grass` weighted simple-block feature and its two placements.
namespace dryGrassDecoration {

[[nodiscard]] const BiomeVegetalDecoration& desert();
[[nodiscard]] const BiomeVegetalDecoration& badlands();
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace dryGrassDecoration

/// Shared forest, mountain, river and plains bush and grass placed features.
namespace forestGroundDecoration {

/// `patch_bush`: rarity 1/4, then 24 candidates with 5/3 triangle offsets.
[[nodiscard]] const BiomeVegetalDecoration& bush();

/// `patch_grass_forest`: two outer attempts, then 32 candidates each.
[[nodiscard]] const BiomeVegetalDecoration& grass();

} // namespace forestGroundDecoration

/// Savanna tall-grass pre-pass and post-tree grass patches.
namespace savannaGroundDecoration {

/// `patch_tall_grass`, used before trees by non-windswept savannas.
[[nodiscard]] const BiomeVegetalDecoration& tallGrass();

/// `patch_grass_savanna`, twenty outer attempts after trees and warm flowers.
[[nodiscard]] const BiomeVegetalDecoration& savanna();

/// `patch_grass_normal`, five outer attempts in windswept savanna.
[[nodiscard]] const BiomeVegetalDecoration& windswept();

} // namespace savannaGroundDecoration

/// Desert ground cover: `patch_dry_grass_desert`, then `patch_dead_bush_2`.
namespace desertDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

/// The patch at one origin, which is what `/place feature` runs for `patch_dead_bush`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace desertDecoration

/// Badlands ground cover: `patch_dry_grass_badlands`, then `patch_dead_bush_badlands`.
namespace badlandsDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

} // namespace badlandsDecoration

} // namespace basalt::server::world::terrain::decoration
