#pragma once

#include <basalt/server/entity/Geometry.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::server::entity::projectile {

/// parity: `Mth.atan2`, a table lookup and a cubic correction rather than the libm one.
[[nodiscard]] double atan2(double y, double x);

struct Rotation {
    float xRotO = 0.0F;
    float yRotO = 0.0F;
    float xRot = 0.0F;
    float yRot = 0.0F;
};

/// parity: `ProjectileUtil.rotateTowardsMovement`; empty when the projectile is at rest.
[[nodiscard]] std::optional<Rotation> rotateTowardsMovement(double motionX, double motionY, double motionZ, float xRotO,
                                                            float yRotO, float xRot, float yRot, float speed);

struct Candidate {
    geometry::Box box;
    float pickRadius = 0.0F;
    bool pickableFromInside = false;
    std::int32_t rootVehicle = 0;
};

struct Hit {
    std::int32_t index = -1;
    geometry::Point at{};
};

/// parity: `ProjectileUtil.getEntityHitResult(Level, Entity, from, to, area, filter, margin)`.
[[nodiscard]] Hit closestHit(std::span<const Candidate> candidates, const geometry::Point& from,
                             const geometry::Point& to, float margin);

/// parity: `ProjectileUtil.getEntityHitResult(Entity, from, to, box, filter, maxValue)`, which prefers other vehicles.
[[nodiscard]] Hit pickedHit(std::span<const Candidate> candidates, std::int32_t exceptRootVehicle,
                            const geometry::Point& from, const geometry::Point& to, double maxValue);

/// parity: `ProjectileUtil.computeMargin`, the grace an arrow grows over its first twenty ticks.
[[nodiscard]] float margin(std::int32_t tickCount);

/// parity: `Shulker.getProgressDeltaAabb`; `direction` is the ordinal of `Direction` (down, up, north, south, west,
/// east).
[[nodiscard]] geometry::Box shulkerProgressBox(float size, std::int32_t direction, float progressFrom, float progressTo,
                                               const geometry::Point& position);

} // namespace basalt::server::entity::projectile
