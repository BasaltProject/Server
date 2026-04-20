#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

namespace vineGrowth {

inline constexpr std::int32_t kOdds = 4;

inline constexpr std::int32_t kCrowd = 5;

/// parity: `VineBlock.randomTick`.
void vine(WorldState& world, const GameRuleValues& gameRules, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace vineGrowth

} // namespace basalt::server::runtime::world
