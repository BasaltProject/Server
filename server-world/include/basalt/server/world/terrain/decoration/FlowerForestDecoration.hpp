#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// The real flower forest decoration: `flower_flower_forest`'s 11-flower noise provider plus its own
/// trees. The preceding `flower_forest_flowers` selector is composed by the biome table.
namespace flowerForestDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

/// The flower-only tail, for when the global tree step already seeded the tree feature.
[[nodiscard]] const BiomeVegetalDecoration& flowersOnly();

/// The flower patch at one origin, which is what `/place feature` runs.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// The noise flower choice at one position, which depends on nothing but the position.
[[nodiscard]] std::int32_t flowerAt(std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace flowerForestDecoration

} // namespace basalt::server::world::terrain::decoration
