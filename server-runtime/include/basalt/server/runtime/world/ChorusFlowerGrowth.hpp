#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `ChorusFlowerBlock.randomTick`.
namespace chorusFlowerGrowth {

/// parity: `ChorusFlowerBlock.DEAD_AGE`, the age a flower that can do nothing else settles on.
inline constexpr std::int32_t kDeadAge = 5;

/// parity: the age past which a flower no longer tries to branch and just dies.
inline constexpr std::int32_t kBranchAge = 4;

/// parity: `ChorusFlowerBlock.randomTick`: it climbs while it has room, branches sideways when it
/// does not, and dies where it can do neither.
///
/// What is not here is `ChorusPlantBlock.getStateWithConnections`: the stem left behind is the
/// plant's default state, so its arms are drawn unconnected until something reshapes it. Nor are
/// the level events 1033 and 1034 vanilla plays over a grown and a dead flower: nothing sends them
/// yet.
void chorusFlower(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                  random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace chorusFlowerGrowth

} // namespace basalt::server::runtime::world
