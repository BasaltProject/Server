#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `SpeleothemBlock.randomTick`, dripstone spikes growing tips and drips.
namespace dripstoneGrowth {

inline constexpr float kChance = 0.011377778F;
inline constexpr std::int32_t kDefaultReach = 7;
inline constexpr std::int32_t kDrop = 10;

using LevelEvent = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t type)>;

/// parity: `PointedDripstoneBlock.randomTick`.
void dripstone(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes, bool evaporates = false,
               const LevelEvent& levelEvent = {});

} // namespace dripstoneGrowth

} // namespace basalt::server::runtime::world
