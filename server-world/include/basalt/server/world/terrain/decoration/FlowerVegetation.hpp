#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration {

namespace forestFlowerDecoration {

[[nodiscard]] const BiomeVegetalDecoration& normal();

[[nodiscard]] const BiomeVegetalDecoration& flowerForest();

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace forestFlowerDecoration

namespace mushroomDecoration {

[[nodiscard]] const BiomeVegetalDecoration& normal();

[[nodiscard]] const BiomeVegetalDecoration& taiga();

[[nodiscard]] const BiomeVegetalDecoration& oldGrowth();

[[nodiscard]] const BiomeVegetalDecoration& swamp();

} // namespace mushroomDecoration

namespace jungleGrassDecoration {

[[nodiscard]] const BiomeVegetalDecoration& decoration();

void placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace jungleGrassDecoration

} // namespace basalt::server::world::terrain::decoration
