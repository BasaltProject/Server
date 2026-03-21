#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {

/// A sapling becoming a tree.
///
/// parity: `SaplingBlock.advanceTree` over `TreeGrower.growTree` with worldgen's own features. Grown
/// on a scratch chunk centred at local 8 and then copied in: the features mask X/Z with `& 15`.
namespace saplingGrowth {

/// parity: `SaplingBlock.randomTick`, which needs light nine above it and rolls one in seven.
inline constexpr std::int32_t kGrowthLight = 9;
inline constexpr std::int32_t kGrowthOdds = 7;

[[nodiscard]] bool isSapling(std::int32_t stateId);

/// Whether a block is in `#minecraft:flowers`, which grows a sapling its bee nest variant.
using Flowering = std::function<bool(const wire::common::Identifier& block)>;

/// One sapling's turn: it grows a stage, or becomes a tree. Answers every block that changed.
///
/// parity: `SaplingBlock.advanceTree`: stage zero only advances the stage, and a sapling that has
/// no room stays where it is rather than half a tree appearing.
[[nodiscard]] std::vector<BlockChange> advance(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, random::RandomSource& random,
                                               const Flowering& flowering = {});

} // namespace saplingGrowth

} // namespace basalt::server::runtime::world
