#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>

#include <optional>

namespace basalt::server::runtime::world {

/// The blocks that grab hold of whatever moves through them.
///
/// parity: `Entity.stuckSpeedMultiplier` and the `makeStuckInBlock` calls that set it. Two blocks
/// catch something that is not alive: the cobweb and powder snow. The sweet berry bush only catches
/// living things, so nothing this build moves by itself ever feels it.
namespace stuckSpeed {

/// What the next move gets multiplied by. All zeroes means nothing is holding on.
struct Multiplier {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/// parity: the `lengthSqr() > 1.0E-7` that decides whether `Entity.move` spends the multiplier.
[[nodiscard]] bool holds(const Multiplier& multiplier);

/// What caught a box that ended this tick's move at `box`, having travelled `dx, dy, dz` to get
/// there, or nothing.
///
/// parity: `Entity.checkInsideBlocks` walks the blocks the box swept through, not only the ones it
/// ended in, so a cobweb catches something that fell straight past it.
[[nodiscard]] std::optional<Multiplier> caughtBy(const entity::entityCollision::BlockAt& blockAt,
                                                 const entity::Aabb& box, double dx, double dy, double dz);

} // namespace stuckSpeed

} // namespace basalt::server::runtime::world
