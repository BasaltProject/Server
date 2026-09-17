#pragma once

#include <basalt/server/runtime/entity/DragonState.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::entity::enderDragon {

/// parity: `Mth.atan2`, which is not libm's and turns a dragon by slightly different amounts.
[[nodiscard]] double vanillaAtan2(double y, double x);

/// parity: `Mth.wrapDegrees`, with Java's remainder.
[[nodiscard]] float wrapDegrees(float degrees) noexcept;

[[nodiscard]] double wrapDegrees(double degrees) noexcept;

/// Lays out the 24 nodes the first time a dragon asks. `heightNoLeaves` is the column's
/// `MOTION_BLOCKING_NO_LEAVES` height.
void buildNodes(DragonState& state, const std::function<std::int32_t(std::int32_t x, std::int32_t z)>& heightNoLeaves);

/// parity: `EnderDragon.findClosestNode(x, y, z)`.
[[nodiscard]] std::int32_t closestNode(const DragonState& state, double x, double y, double z, bool crystalsLeft);

/// parity: `EnderDragon.findPath`, an A* over the node graph that may end on one extra node.
[[nodiscard]] std::optional<Path> findPath(DragonState& state, std::int32_t startIndex, std::int32_t endIndex,
                                           std::optional<std::array<std::int32_t, 3>> finalNode, bool crystalsLeft);

} // namespace basalt::server::runtime::entity::enderDragon
