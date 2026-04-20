#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

namespace chorusFlowerGrowth {

inline constexpr std::int32_t kDeadAge = 5;

inline constexpr std::int32_t kBranchAge = 4;

/// parity: `ChorusFlowerBlock.randomTick`.
void chorusFlower(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace chorusFlowerGrowth

} // namespace basalt::server::runtime::world
