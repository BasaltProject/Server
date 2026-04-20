#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

namespace lavaGrowth {

/// parity: `LavaFluid.randomTick`.
void lava(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
          std::vector<BlockChange>& changes);

} // namespace lavaGrowth

} // namespace basalt::server::runtime::world
