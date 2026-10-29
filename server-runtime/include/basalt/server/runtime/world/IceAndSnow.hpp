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

/// One roll that hit: the column at (x, z) freezes its top water and, in rain, gains a layer of snow.
void precipitate(LoadedWorld& world, std::int32_t x, std::int32_t z, std::vector<BlockChange>& changes);

} // namespace iceAndSnow

} // namespace basalt::server::runtime::world
