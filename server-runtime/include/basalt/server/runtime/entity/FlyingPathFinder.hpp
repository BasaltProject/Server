#pragma once

#include <basalt/server/runtime/entity/PathTypes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// A way through the air from one place to another.
/// parity: `PathFinder` over `FlyNodeEvaluator`: A* over the squares the mob fits in, 26 neighbours
/// (diagonals only past enterable corners), each priced by the mob's `PathType` malus (one more over
/// the ground), heuristic weighted by 1.5. An unreachable target gives the path to the closest square.
namespace flyingPathFinder {

using Node = std::array<std::int32_t, 3>;

/// The size of the mob in whole squares. parity: `floor(width + 1)` across and `floor(height + 1)` up.
struct Footprint {
    std::int32_t width = 1;
    std::int32_t height = 1;
};

/// What the mob itself brings to the search: its costs, where it stands and the world's floor.
struct Mob {
    pathTypes::Maluses maluses = pathTypes::defaults();
    std::optional<Node> at;
    std::int32_t minY = -64;
};

/// The squares from the one after the start to the last one, empty when it is already there or has
/// nowhere to go. `maxNodes` is `followRange * 16` and `reach` the follow range itself.
[[nodiscard]] std::vector<Node> find(world::WorldState& world, const Footprint& size, const Node& from, const Node& to,
                                     std::int32_t maxNodes, double reach, const Mob& mob = {});

} // namespace flyingPathFinder

} // namespace basalt::server::runtime::entity
