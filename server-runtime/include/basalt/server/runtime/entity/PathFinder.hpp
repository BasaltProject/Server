#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::runtime::entity {

namespace pathFinder {

/// parity: `WalkNodeEvaluator`.
inline constexpr std::int32_t kMaxDrop = 3;

/// How many nodes one search may open before it gives up. parity: `PathFinder`'s own 200.
inline constexpr std::int32_t kMaxNodes = 200;

[[nodiscard]] std::optional<std::vector<std::array<std::int32_t, 3>>> find(world::WorldState& world, std::int32_t fromX,
                                                                           std::int32_t fromY, std::int32_t fromZ,
                                                                           std::int32_t toX, std::int32_t toY,
                                                                           std::int32_t toZ);

struct RouteOptions {
    std::int32_t accuracy = 0;
    float maxPathLength = 16.0F;
    std::int32_t maxVisited = 256;
    bool openDoors = false;
    bool amphibious = false;
};

struct Route {
    std::vector<std::array<std::int32_t, 3>> nodes;
    std::array<std::int32_t, 3> target{};
    bool reached = false;
};

[[nodiscard]] std::optional<Route> findRoute(world::WorldState& world, std::int32_t fromX, std::int32_t fromY,
                                             std::int32_t fromZ, std::span<const std::array<std::int32_t, 3>> targets,
                                             const RouteOptions& options);

/// parity: `WalkNodeEvaluator.getStart`.
[[nodiscard]] std::array<std::int32_t, 3> startOf(world::WorldState& world, double x, double y, double z,
                                                  bool onGround);

[[nodiscard]] std::array<std::int32_t, 3> amphibiousStartOf(world::WorldState& world, double x, double y, double z,
                                                            double width, bool onGround, bool inWater);

[[nodiscard]] bool isMobDoor(std::int32_t stateId);

[[nodiscard]] bool canStandAt(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace pathFinder

} // namespace basalt::server::runtime::entity
