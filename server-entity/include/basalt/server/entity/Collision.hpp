#pragma once

#include <basalt/server/entity/Geometry.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::server::entity::collision {

/// A box of a collision shape; the early exit of a sweep happens between shapes, never inside one.
struct Collider {
    geometry::Box box;
    std::int32_t shape = 0;
};

/// parity: `Entity.collideWithShapes`, which sweeps Y first and then the longer of X and Z.
[[nodiscard]] geometry::Point sweep(std::span<const Collider> colliders, const geometry::Box& box,
                                    const geometry::Point& movement);

/// parity: `Entity.collide`'s step-up, the first rise of the sorted candidates that gets further than `reach`.
[[nodiscard]] std::optional<geometry::Point> stepUp(std::span<const Collider> colliders, const geometry::Box& grounded,
                                                    double groundedOffsetY, const geometry::Point& movement,
                                                    float maxUpStep, double reachSquared);

} // namespace basalt::server::entity::collision
