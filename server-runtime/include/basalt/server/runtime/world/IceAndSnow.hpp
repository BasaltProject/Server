#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// parity: `ServerLevel.tickPrecipitation`, which freezes still water and piles snow on cold ground.
namespace iceAndSnow {

/// The first half of `ServerLevel.tickChunk`: `speed` rolls of one in forty-eight per chunk.
[[nodiscard]] std::vector<BlockChange> tick(LoadedWorld& world, std::span<const std::array<std::int32_t, 2>> chunks,
                                            std::int32_t speed, random::RandomSource& random);

} // namespace iceAndSnow

} // namespace basalt::server::runtime::world
