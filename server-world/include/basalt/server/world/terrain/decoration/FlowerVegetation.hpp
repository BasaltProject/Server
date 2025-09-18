#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration {

/// The two `forest_flowers` simple-random-selector placed-feature variants.
namespace forestFlowerDecoration {

/// Standard forests: rarity 1/7 and a clamped uniform(-3..1, 0..1) outer count.
[[nodiscard]] const BiomeVegetalDecoration& normal();

/// Flower forest: rarity 1/7 and a clamped uniform(-1..3, 0..3) outer count.
[[nodiscard]] const BiomeVegetalDecoration& flowerForest();

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// The flower patch at one origin, which is what `/place feature` runs.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace forestFlowerDecoration

/// Shared small-mushroom placed features used by Overworld biomes.
namespace mushroomDecoration {

/// `addDefaultMushrooms`: brown 1/256, then red 1/512.
[[nodiscard]] const BiomeVegetalDecoration& normal();

/// The mushroom tail of `addTaigaGrass`: brown 1/4, then red 1/256.
[[nodiscard]] const BiomeVegetalDecoration& taiga();

/// Old-growth mushrooms, followed separately by `normal` in those biomes.
[[nodiscard]] const BiomeVegetalDecoration& oldGrowth();

/// Swamp mushrooms, followed separately by `normal`.
[[nodiscard]] const BiomeVegetalDecoration& swamp();

} // namespace mushroomDecoration

/// `patch_grass_jungle`. The weighted provider is short_grass 3, fern 1, so `nextInt(4) < 3` picks
/// short grass.
namespace jungleGrassDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

void placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace jungleGrassDecoration

} // namespace basalt::server::world::terrain::decoration
