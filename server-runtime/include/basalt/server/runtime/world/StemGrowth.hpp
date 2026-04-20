#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `StemBlock.randomTick`, pumpkin and melon stems.
namespace stemGrowth {

inline constexpr std::int32_t kMaxAge = 7;

/// parity: `StemBlock.randomTick`.
void stem(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, float speed,
          random::RandomSource& random, std::vector<BlockChange>& changes, bool hasSkylight = true);

} // namespace stemGrowth

/// parity: `CocoaBlock.randomTick`, one roll in five, three ages.
namespace cocoaGrowth {

inline constexpr std::int32_t kOdds = 5;
inline constexpr std::int32_t kMaxAge = 2;

void cocoa(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
           random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace cocoaGrowth

} // namespace basalt::server::runtime::world
