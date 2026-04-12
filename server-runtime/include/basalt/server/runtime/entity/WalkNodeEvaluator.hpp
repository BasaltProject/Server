#pragma once

#include <basalt/server/runtime/entity/PathTypes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// A way over the ground. parity: `PathFinder` over `WalkNodeEvaluator`: A* on vanilla's binary heap,
/// each square judged through the mob's whole box and priced by its maluses, and a path that ends at
/// the node that came nearest whether it got there or not.
namespace walkNodes {

using Position = std::array<std::int32_t, 3>;

/// What the mob brings to the search. parity: the `Mob` the evaluator is prepared with and the flags its
/// navigation set on it.
struct WalkMob {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float width = 0.6F;
    float height = 1.8F;
    bool onGround = false;
    /// parity: `Entity.isInFloatableFluid`.
    bool inFloatableFluid = false;
    /// parity: `LivingEntity.maxUpStep`.
    float maxUpStep = 0.6F;
    /// parity: `Mob.getMaxFallDistance`.
    std::int32_t maxFallDistance = 3;
    pathTypes::Maluses maluses = pathTypes::defaults();
    bool canFloat = false;
    bool canPassDoors = true;
    bool canOpenDoors = false;
    bool canWalkOverFences = false;
    std::int32_t minY = -64;
    /// parity: `Creaking.HomeNodeEvaluator`: a square more than thirty-two blocks from here, and no nearer
    /// to it than the mob already is, is blocked.
    std::optional<Position> home;
    /// parity: `PowderSnowBlock.canEntityWalkOnPowderSnow`, leather boots or the tag; with it the
    /// snow below its feet is solid to it. Its fall and crouch decide the rest of the snow's shape.
    bool walksOnPowderSnow = false;
    double fallDistance = 0.0;
    bool descending = false;
    /// The world border as min x, min z, max x, max z. parity: `noBorderCollision` for the mob.
    std::optional<std::array<double, 4>> border;
};

/// parity: `Node`, as far as a path keeps it.
struct Node {
    Position at{};
    pathTypes::PathType type = pathTypes::PathType::Blocked;
};

/// parity: `Path`: from the start node to the one nearest the target.
struct Path {
    std::vector<Node> nodes;
    Position target{};
    /// parity: `Path.canReach`.
    bool reached = false;
};

/// parity: `PathFinder.findPath` for one target, `maxVisitedNodes` being the finder's own budget.
[[nodiscard]] Path findPath(world::WorldState& world, const WalkMob& mob, const Position& target, float maxPathLength,
                            std::int32_t reachRange, std::int32_t maxVisitedNodes);

/// parity: the static `WalkNodeEvaluator.getFloorLevel`: the top of the collision shape under the square.
[[nodiscard]] double floorLevel(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace walkNodes

} // namespace basalt::server::runtime::entity
