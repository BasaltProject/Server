#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `GrowingPlantHeadBlock.randomTick`.
namespace climbingPlantGrowth {

inline constexpr std::int32_t kMaxAge = 25;

void growingPlant(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool upwards,
                  double chance, random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace climbingPlantGrowth

/// parity: `ChangeOverTimeBlock.changeOverTime`.
namespace weatheringGrowth {

inline constexpr float kDailyChance = 0.05688889F;

/// parity: `WeatheringCopper.getChanceModifier`.
inline constexpr float kUnaffectedChanceModifier = 0.75F;

inline constexpr std::int32_t kRange = 4;

/// parity: `changeOverTime`.
void weather(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
             random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace weatheringGrowth

} // namespace basalt::server::runtime::world
