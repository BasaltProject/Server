#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

namespace mushroomGrowth {

inline constexpr std::int32_t kOdds = 25;

inline constexpr std::int32_t kCrowd = 5;

/// parity: `MushroomBlock.randomTick`.
void mushroom(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
              random::RandomSource& random, std::vector<BlockChange>& changes,
              const blockSurvival::ScaffoldingHasFallen& scaffoldingHasFallen = {}, bool hasSkylight = true);

} // namespace mushroomGrowth

} // namespace basalt::server::runtime::world
