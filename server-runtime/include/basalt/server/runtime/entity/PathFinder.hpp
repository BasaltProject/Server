#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// A way through the blocks from one place to another.
///
/// parity: `PathFinder` over `WalkNodeEvaluator`: A* over the positions a mob can stand in, with the
/// heuristic weighted by 1.5 as vanilla weights it, a step up of one block, a drop of at most three,
/// and diagonals only when both of the blocks beside them are open. What a mob does with the path is
/// `PathNavigation`'s business, and here the caller's.
namespace pathFinder {

/// parity: `WalkNodeEvaluator`, a drop a mob will take without looking for another way round.
inline constexpr std::int32_t kMaxDrop = 3;

/// How many nodes one search may open before it gives up. parity: `PathFinder`'s own 200.
inline constexpr std::int32_t kMaxNodes = 200;

/// The path as the blocks to walk through, from the first step to the target, or nothing when there
/// is no way there. An empty path means it is already standing on the target.
[[nodiscard]] std::optional<std::vector<std::array<std::int32_t, 3>>> find(world::WorldState& world, std::int32_t fromX,
                                                                           std::int32_t fromY, std::int32_t fromZ,
                                                                           std::int32_t toX, std::int32_t toY,
                                                                           std::int32_t toZ);

/// parity: `WalkNodeEvaluator`: something to stand on, and two blocks of room to stand in.
[[nodiscard]] bool canStandAt(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace pathFinder

} // namespace basalt::server::runtime::entity
