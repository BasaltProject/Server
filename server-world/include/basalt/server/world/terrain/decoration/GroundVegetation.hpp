#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration {

/// their rarity filter. parity: the weighted provider is poppy 2, dandelion 1, so `nextInt(3) < 2`
namespace defaultVegetationDecoration {

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

void placeDefaultPatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace defaultVegetationDecoration

namespace dryGrassDecoration {

[[nodiscard]] const BiomeVegetalDecoration& desert();
[[nodiscard]] const BiomeVegetalDecoration& badlands();
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace dryGrassDecoration

namespace forestGroundDecoration {

[[nodiscard]] const BiomeVegetalDecoration& bush();

[[nodiscard]] const BiomeVegetalDecoration& grass();

} // namespace forestGroundDecoration

namespace savannaGroundDecoration {

[[nodiscard]] const BiomeVegetalDecoration& tallGrass();

[[nodiscard]] const BiomeVegetalDecoration& savanna();

[[nodiscard]] const BiomeVegetalDecoration& windswept();

} // namespace savannaGroundDecoration

namespace desertDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace desertDecoration

namespace badlandsDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

} // namespace badlandsDecoration

} // namespace basalt::server::world::terrain::decoration
