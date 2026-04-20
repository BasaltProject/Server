#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

namespace villageFeatures {

void place(std::string_view feature, Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z,
           random::RandomSource& random);

/// parity: `BlockPileFeature.mayPlaceOn`, which a dirt path passes only on a coin flip.
[[nodiscard]] bool mayPlacePileOn(std::int32_t belowStateId, std::int32_t dirtPathStateId,
                                  random::RandomSource& random);

} // namespace villageFeatures

} // namespace basalt::server::world::terrain::decoration
