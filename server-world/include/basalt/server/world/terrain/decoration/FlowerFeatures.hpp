#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

namespace paleGardenDecoration {

[[nodiscard]] const BiomeVegetalDecoration& flowerPatch();

[[nodiscard]] const BiomeVegetalDecoration& rareFlower();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

void placeSingleAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace paleGardenDecoration

namespace cherryGroveDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

[[nodiscard]] const BiomeVegetalDecoration& flowersOnly();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

[[nodiscard]] std::int32_t pinkPetalsStateId(std::int32_t choice);

} // namespace cherryGroveDecoration

} // namespace basalt::server::world::terrain::decoration
