#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/TheEndBiomeSource.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration::end {

/// parity: `end_island_decorated`, the End's raw generation placed feature.
namespace endRawDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const TheEndBiomeSource& biomeSource);

void placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace endRawDecoration

/// parity: `end_platform`, the End's fixed top layer modification feature.
namespace endTopLayerDecoration {

void decorate(Chunk& chunk, const TheEndBiomeSource& biomeSource);

} // namespace endTopLayerDecoration

/// parity: `chorus_plant`, the End highlands vegetal decoration feature.
namespace endVegetalDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const NoiseRouter& router, const TheEndBiomeSource& biomeSource);

bool placeAt(Chunk& chunk, random::RandomSource& random, const NoiseRouter& router, std::int32_t x, std::int32_t y,
             std::int32_t z);

} // namespace endVegetalDecoration

/// parity: the End's surface structures step, the return gateways and the ten obsidian spikes.
namespace endSurfaceStructuresDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const NoiseRouter& router, const TheEndBiomeSource& biomeSource);

bool placeSpikesIn(Chunk& chunk, std::int64_t seed);

} // namespace endSurfaceStructuresDecoration

} // namespace basalt::server::world::terrain::decoration::end
