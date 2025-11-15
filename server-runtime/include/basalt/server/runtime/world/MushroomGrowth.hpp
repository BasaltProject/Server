#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `MushroomBlock.randomTick`.
namespace mushroomGrowth {

inline constexpr std::int32_t kOdds = 25;

/// parity: no more than five of its kind in the nine by three by nine box around it.
inline constexpr std::int32_t kCrowd = 5;

/// parity: `MushroomBlock.randomTick`: one roll in twenty-five, no more than five of its kind in a
/// nine by three by nine box, and a walk of four hops to find somewhere dark enough to sit.
void mushroom(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
              random::RandomSource& random, std::vector<BlockChange>& changes,
              const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen = {}, bool hasSkylight = true);

} // namespace mushroomGrowth

} // namespace basalt::server::runtime::world
