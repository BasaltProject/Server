#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration::nether {

/// parity: `HugeFungusFeature` with the planted crimson or warped configuration.
namespace hugeFungusFeature {

bool place(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z, bool warped);

} // namespace hugeFungusFeature

} // namespace basalt::server::world::terrain::decoration::nether
