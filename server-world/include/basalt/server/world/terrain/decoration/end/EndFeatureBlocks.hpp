#pragma once

#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>

#include <cstdint>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::world::terrain::decoration::end {

/// parity: `EndGatewayFeature.place`.
namespace endGatewayFeature {

void placeBlocks(std::int32_t x, std::int32_t y, std::int32_t z, const endSpikes::SetBlock& set);

} // namespace endGatewayFeature

/// parity: `EndIslandFeature.place`, one blob of end stone narrowing downwards.
namespace endIslandFeature {

void placeBlocks(random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                 const endSpikes::SetBlock& set);

} // namespace endIslandFeature

} // namespace basalt::server::world::terrain::decoration::end
