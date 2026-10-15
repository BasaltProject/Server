#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>

#include <optional>

namespace basalt::server::runtime::world {

/// parity: `Entity.stuckSpeedMultiplier`.
namespace stuckSpeed {

/// All zeroes means nothing is holding on.
struct Multiplier {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// parity: the `lengthSqr() > 1.0E-7` that decides whether `Entity.move` spends the multiplier.
[[nodiscard]] bool holds(const Multiplier& multiplier);

/// parity: `Entity.checkInsideBlocks`.
[[nodiscard]] std::optional<Multiplier> caughtBy(const entity::entityCollision::BlockAt& blockAt,
                                                 const entity::Aabb& box, double dx, double dy, double dz);

} // namespace stuckSpeed

} // namespace basalt::server::runtime::world
