#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `LavaFluid.randomTick`, lava setting fire to what it can reach.
namespace lavaGrowth {

/// parity: `LavaFluid.randomTick`: lava sets light to the air above what will burn, and reaches up
/// to three blocks up when it has room.
void lava(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
          std::vector<BlockChange>& changes);

} // namespace lavaGrowth

} // namespace basalt::server::runtime::world
