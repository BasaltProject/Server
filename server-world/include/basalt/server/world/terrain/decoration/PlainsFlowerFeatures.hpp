#pragma once

#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// The plains flower patch, whose provider mixes a fixed noise with the feature's own random.
namespace plainsDecoration {

/// The patch at one origin, which is what `/place feature` runs for `flower_plains`.
void placeFlowerPlainAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z);

/// Which flower the provider picks. Reads the noise at the position and then draws, so it consumes
/// the random and is not a pure function of the coordinates.
[[nodiscard]] std::int32_t flowerStateId(random::RandomSource& random, const DecorationPos& origin);

} // namespace plainsDecoration

/// The wildflower bed the birch forest and meadow both scatter.
namespace forestFloorDecoration {

/// `count` differs per caller: 64 for the birch forest, 8 for the meadow.
void placeWildflowersAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::int32_t count);

/// One of the 16 packed flower-bed states, by the weighted provider's choice index.
[[nodiscard]] std::int32_t flowerBedStateId(std::int32_t defaultStateId, std::int32_t choice);

} // namespace forestFloorDecoration

} // namespace basalt::server::world::terrain::decoration
