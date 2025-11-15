#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `SpeleothemBlock.randomTick`, dripstone spikes growing tips and drips.
namespace dripstoneGrowth {

/// parity: `SpeleothemBlock.randomTick`'s own chance, its reach and its drop.
inline constexpr float kChance = 0.011377778F;
inline constexpr std::int32_t kDefaultReach = 7;
inline constexpr std::int32_t kDrop = 10;

/// parity: `SpeleothemBlock.randomTick` over `growStalactiteOrStalagmiteIfPossible`: a stalactite
/// hanging under dripstone grows a tip down, or drips a stalagmite up from the floor below it.
///
/// What is not here is the other half of `PointedDripstoneBlock.randomTick`, which fills a cauldron
/// under it, and which waits for cauldrons to hold anything.
void dripstone(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
               random::RandomSource& random, std::vector<BlockChange>& changes);

} // namespace dripstoneGrowth

} // namespace basalt::server::runtime::world
