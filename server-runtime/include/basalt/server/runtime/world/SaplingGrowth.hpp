#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// A sapling becoming a tree.
///
/// parity: `SaplingBlock.advanceTree` over `TreeGrower.growTree`, using the very features worldgen
/// plants, so a grown tree has the same shape as one the world came with.
///
/// Grown on a scratch chunk with the sapling at its middle and only then copied into the world: the
/// decoration features mask their X/Z with `& 15`, so a canopy reaching the next chunk would wrap
/// back into this one. Centred at local 8, nothing vanilla plants is wide enough to reach an edge.
namespace saplingGrowth {

/// parity: `SaplingBlock.randomTick`, which needs light nine above it and rolls one in seven.
inline constexpr std::int32_t kGrowthLight = 9;
inline constexpr std::int32_t kGrowthOdds = 7;

[[nodiscard]] bool isSapling(std::int32_t stateId);

/// One sapling's turn: it grows a stage, or becomes a tree. Answers every block that changed.
///
/// parity: `SaplingBlock.advanceTree`: stage zero only advances the stage, and a sapling that has
/// no room stays where it is rather than half a tree appearing.
[[nodiscard]] std::vector<BlockChange> advance(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, random::RandomSource& random);

} // namespace saplingGrowth

} // namespace basalt::server::runtime::world
