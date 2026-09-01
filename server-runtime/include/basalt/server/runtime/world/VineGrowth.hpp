#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `VineBlock.randomTick`.
namespace vineGrowth {

inline constexpr std::int32_t kOdds = 4;

/// parity: `VineBlock.canSpread`, five of its kind in a nine by three by nine box.
inline constexpr std::int32_t kCrowd = 5;

/// parity: `VineBlock.randomTick`, under the `spread_vines` game rule: one roll in four picks a
/// direction, and a vine only spreads while fewer than five of its kind stand within four blocks.
///
/// Its full walk carries a vine round corners and up over ledges; what is here is the two moves
/// that make a wall grow over: taking a face beside it, and climbing to the block above.
void vine(WorldState& world, const GameRuleValues& gameRules, std::int32_t x, std::int32_t y, std::int32_t z,
          std::int32_t stateId, random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace vineGrowth

} // namespace basalt::server::runtime::world
