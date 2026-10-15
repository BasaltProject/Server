#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// parity: `Entity.collide` over `Shapes.collide`.
namespace entityCollision {

using BlockAt = std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z)>;

using ShapeFor =
    std::function<std::optional<Aabb>(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: the 1.0E-7 vanilla treats as no movement at all, and as the slack on a shared edge.
inline constexpr double kEpsilon = 1.0e-7;

inline constexpr std::int32_t kMaxSpan = 8;

[[nodiscard]] std::array<double, 3> collide(const BlockAt& blockAt, const Aabb& box, double dx, double dy, double dz,
                                            const ShapeFor& shapeFor = {});

/// parity: `Entity.collide`.
[[nodiscard]] std::array<double, 3> collideWithStep(const BlockAt& blockAt, const Aabb& box, double dx, double dy,
                                                    double dz, double stepHeight, bool onGround);

/// parity: `Level.noCollision`, inverted.
[[nodiscard]] bool collides(const BlockAt& blockAt, const Aabb& box);

[[nodiscard]] std::vector<Aabb> collidingBoxes(const BlockAt& blockAt, const Aabb& box, const ShapeFor& shapeFor = {});

} // namespace entityCollision

} // namespace basalt::server::runtime::entity
