#pragma once

#include <basalt/server/runtime/entity/PathTypes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// A way over the ground. parity: `PathFinder` over `WalkNodeEvaluator`: A* on vanilla's binary heap.
namespace walkNodes {

using Position = std::array<std::int32_t, 3>;

struct WalkMob {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float width = 0.6F;
    float height = 1.8F;
    bool onGround = false;
    bool inFloatableFluid = false;
    float maxUpStep = 0.6F;
    std::int32_t maxFallDistance = 3;
    pathTypes::Maluses maluses = pathTypes::defaults();
    bool canFloat = false;
    bool canPassDoors = true;
    bool canOpenDoors = false;
    bool canWalkOverFences = false;
    std::int32_t minY = -64;
    std::optional<Position> home;
    bool walksOnPowderSnow = false;
    double fallDistance = 0.0;
    bool descending = false;
    std::optional<std::array<double, 4>> border;
};

struct Node {
    Position at{};
    pathTypes::PathType type = pathTypes::PathType::Blocked;
};

struct Path {
    std::vector<Node> nodes;
    Position target{};
    bool reached = false;
};

[[nodiscard]] Path findPath(world::WorldState& world, const WalkMob& mob, const Position& target, float maxPathLength,
                            std::int32_t reachRange, std::int32_t maxVisitedNodes);

[[nodiscard]] double floorLevel(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace walkNodes

} // namespace basalt::server::runtime::entity
