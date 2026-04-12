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

/// parity: `SpeleothemBlock.randomTick`'s own chance, its reach and its drop.
inline constexpr float kChance = 0.011377778F;
inline constexpr std::int32_t kDefaultReach = 7;
inline constexpr std::int32_t kDrop = 10;

/// Where a block's level event goes: the drip a stalactite plays at its tip.
using LevelEvent = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t type)>;

/// parity: `PointedDripstoneBlock.randomTick`: the drip first, which dries mud to clay or books the
/// cauldron under it a turn, then `SpeleothemBlock.randomTick` over
/// `growStalactiteOrStalagmiteIfPossible`: a stalactite grows a tip down, or drips a stalagmite up.
/// `evaporates` is the dimension's `water_evaporates`.
void dripstone(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes, bool evaporates = false,
               const LevelEvent& levelEvent = {});

} // namespace dripstoneGrowth

} // namespace basalt::server::runtime::world
