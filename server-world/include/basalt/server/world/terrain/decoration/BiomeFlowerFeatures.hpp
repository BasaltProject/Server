#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// The flower forest's noise-selected flower patch and its own trees.
namespace flowerForestDecoration {

/// The trees followed by the flower patch, which is the order the biome runs them in.
void decorate(Chunk& chunk, random::RandomSource& random);

/// The flower-only tail, for when the global tree step already seeded the tree feature.
[[nodiscard]] const BiomeVegetalDecoration& flowersOnly();

/// The patch at one origin, which is what `/place feature` runs for `flower_forest_flowers`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// Which of the eleven flowers the noise picks at a position.
[[nodiscard]] std::int32_t flowerAt(std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace flowerForestDecoration

/// The swamp's blue orchid patch.
namespace swampDecoration {

/// The patch at one origin, which is what `/place feature` runs for `flower_swamp`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace swampDecoration

/// The meadow's flower patch, whose provider draws from two noises rather than the random, plus its
/// grass and its rare trees.
namespace meadowDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

/// The non-tree tail, for when the global tree step already owns `trees_meadow`.
[[nodiscard]] const BiomeVegetalDecoration& withoutTrees();

/// parity: `trees_meadow`, rarity 1/100 and a 50/50 fancy oak against super birch selector.
[[nodiscard]] const BiomeVegetalDecoration& trees();

[[nodiscard]] const BiomeVegetalDecoration& flowers();

[[nodiscard]] const BiomeVegetalDecoration& grass();

/// The patch at one origin, which is what `/place feature` runs for `flower_meadow`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// Which flower the two noises pick at a position. Tall grass comes back as its lower half.
[[nodiscard]] std::int32_t flowerStateAt(const DecorationPos& position);

} // namespace meadowDecoration

} // namespace basalt::server::world::terrain::decoration
