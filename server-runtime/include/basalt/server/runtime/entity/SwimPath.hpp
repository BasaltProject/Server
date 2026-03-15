#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// A way through the water from one place to another.
///
/// parity: `PathFinder` over `SwimNodeEvaluator`: the six faces and the four flat corners, water
/// only, and for a dolphin the air right above the surface too.
namespace swimPath {

/// parity: `PathNavigation`'s visited-node budget, the follow range times sixteen for a dolphin.
inline constexpr std::int32_t kMaxNodes = 256;

/// Whether this state holds water. parity: `FluidState.is(FluidTags.WATER)`.
[[nodiscard]] bool holdsWater(std::int32_t stateId);

/// parity: `SwimNodeEvaluator.getPathTypeOfMob` answering `WATER`.
[[nodiscard]] bool isSwimmable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: the same answering `BREACH`: air with water under it.
[[nodiscard]] bool isBreach(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The blocks to swim through, first step first. When the target cannot be reached the path ends at
/// the node nearest to it, as vanilla's does; nothing means there is no step to take at all.
[[nodiscard]] std::optional<std::vector<std::array<std::int32_t, 3>>> find(world::WorldState& world,
                                                                           const std::array<std::int32_t, 3>& from,
                                                                           const std::array<std::int32_t, 3>& to,
                                                                           bool breaching);

} // namespace swimPath

} // namespace basalt::server::runtime::entity
