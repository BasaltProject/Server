#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

namespace bambooGrowth {

inline constexpr std::int32_t kOdds = 3;
inline constexpr std::int32_t kMaxHeight = 16;

inline constexpr std::int32_t kTall = 11;
inline constexpr float kStageChance = 0.25F;

void bamboo(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
            random::RandomSource& random, std::vector<BlockChange>& changes, bool hasSkylight = true);

void bambooSapling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                   std::vector<BlockChange>& changes, bool hasSkylight = true);

} // namespace bambooGrowth

} // namespace basalt::server::runtime::world
