#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
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

/// A search that may stop short of its target, and may have several.
struct RouteOptions {
    /// parity: the reach range: a node this many blocks away, counted Manhattan, is there.
    std::int32_t accuracy = 0;
    /// parity: `getMaxPathLength`, how far from the start a node may be.
    float maxPathLength = 16.0F;
    /// parity: the node budget, sixteen per block of that length.
    std::int32_t maxVisited = 256;
    /// parity: `canOpenDoors`, which walks through a wooden or copper door as if it were open.
    bool openDoors = false;
};

/// What a search found. parity: `Path`, whose last node is the target or the nearest it got.
struct Route {
    std::vector<std::array<std::int32_t, 3>> nodes;
    std::array<std::int32_t, 3> target{};
    /// parity: `Path.canReach`.
    bool reached = false;
};

/// parity: `PathFinder.findPath` over a set of targets: the shortest path to a target it reached,
/// or else the one ending nearest to any of them. Nothing when it cannot even start.
[[nodiscard]] std::optional<Route> findRoute(world::WorldState& world, std::int32_t fromX, std::int32_t fromY,
                                             std::int32_t fromZ, std::span<const std::array<std::int32_t, 3>> targets,
                                             const RouteOptions& options);

/// parity: `WalkNodeEvaluator.getStart`: half a block up for a mob on the ground, else the first
/// block under it that is not air.
[[nodiscard]] std::array<std::int32_t, 3> startOf(world::WorldState& world, double x, double y, double z,
                                                  bool onGround);

/// Whether a closed door here is one a mob that opens doors walks through.
[[nodiscard]] bool isMobDoor(std::int32_t stateId);

/// parity: `WalkNodeEvaluator`: something to stand on, and two blocks of room to stand in.
[[nodiscard]] bool canStandAt(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace pathFinder

} // namespace basalt::server::runtime::entity
