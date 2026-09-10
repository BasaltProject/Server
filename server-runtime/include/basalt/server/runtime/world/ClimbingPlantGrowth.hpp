#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `GrowingPlantHeadBlock.randomTick`: kelp, cave vines and the nether vines all climb their
/// own way at their own odds until they reach age twenty-five.
namespace climbingPlantGrowth {

/// parity: `GrowingPlantHeadBlock`'s own twenty-five, past which nothing climbs further.
inline constexpr std::int32_t kMaxAge = 25;

void growingPlant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool upwards,
                  double chance, random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace climbingPlantGrowth

/// parity: `ChangeOverTimeBlock.changeOverTime`, copper and the like weathering over time.
namespace weatheringGrowth {

/// parity: `ChangeOverTimeBlock`'s own "each block once per day" figure.
inline constexpr float kDailyChance = 0.05688889F;

/// parity: `WeatheringCopper.getChanceModifier`, which is this while the block is unaffected and one
/// for every stage after it: fresh copper takes the first step more slowly than the rest.
inline constexpr float kUnaffectedChanceModifier = 0.75F;

/// How far a neighbour counts from, measured the way vanilla measures it.
inline constexpr std::int32_t kRange = 4;

/// parity: `changeOverTime` and its `getNextState`: a block weathers about once a day, more readily
/// the more weathered its neighbours are, and never while one younger than it stands within four
/// blocks.
void weather(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
             random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace weatheringGrowth

} // namespace basalt::server::runtime::world
