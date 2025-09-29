#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// The real meadow vegetation, including `flower_meadow`'s dual-noise state provider and the rare
/// trees.
namespace meadowDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

/// The non-tree tail, for when the global tree step already owns `trees_meadow`.
[[nodiscard]] const BiomeVegetalDecoration& withoutTrees();

/// parity: `trees_meadow`, rarity 1/100 and a 50/50 fancy oak against super birch selector.
[[nodiscard]] const BiomeVegetalDecoration& trees();

[[nodiscard]] const BiomeVegetalDecoration& flowers();

[[nodiscard]] const BiomeVegetalDecoration& grass();

/// The flower patch at one origin, which is what `/place feature` runs for `flower_meadow`.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// The dual-noise flower choice at one position, which depends on nothing but the position.
[[nodiscard]] std::int32_t flowerStateAt(const DecorationPos& position);

} // namespace meadowDecoration

} // namespace basalt::server::world::terrain::decoration
