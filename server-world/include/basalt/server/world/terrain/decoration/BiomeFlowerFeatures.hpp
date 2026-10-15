#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

namespace flowerForestDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

[[nodiscard]] const BiomeVegetalDecoration& flowersOnly();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

[[nodiscard]] std::int32_t flowerAt(std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace flowerForestDecoration

namespace swampDecoration {

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace swampDecoration

namespace meadowDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

[[nodiscard]] const BiomeVegetalDecoration& withoutTrees();

/// parity: `trees_meadow`.
[[nodiscard]] const BiomeVegetalDecoration& trees();

[[nodiscard]] const BiomeVegetalDecoration& flowers();

[[nodiscard]] const BiomeVegetalDecoration& grass();

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

[[nodiscard]] std::int32_t flowerStateAt(const DecorationPos& position);

} // namespace meadowDecoration

} // namespace basalt::server::world::terrain::decoration
