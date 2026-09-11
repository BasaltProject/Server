#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

/// Moving a box through the blocks, one axis at a time.
///
/// parity: `Entity.collide` over `Shapes.collide`. The order is not free: Y first, then the smaller
/// of the two horizontal moves, then the larger (`Direction.axisStepOrder`). Resolving X before Y
/// lets an entity walk into a wall it should have landed on top of, which is exactly the kind of
/// difference that shows up as a mob standing in the air.
namespace entityCollision {

/// What the world says is at a block, or nothing when that block is not loaded.
///
/// A callback rather than the world itself: the collision code needs one question answered, and
/// asking it this way is what keeps the world from having to exist before the physics can be
/// tested.
using BlockAt = std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// The box a block offers this mover, when that is not the one in the shape table. parity:
/// `Block.getCollisionShape` is handed a `CollisionContext`, and powder snow is the one block that
/// reads it: a lid nine tenths high to something falling more than two and a half blocks, not there
/// at all to everything else. Nine tenths does not fit the table's thirty-seconds, so it is doubles.
using ShapeFor =
    std::function<std::optional<Aabb>(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

/// parity: the 1.0E-7 vanilla treats as no movement at all, and as the slack on a shared edge.
inline constexpr double kEpsilon = 1.0e-7;

/// How far a swept box reaches before this gives up gathering blocks, as a sanity bound.
inline constexpr std::int32_t kMaxSpan = 8;

/// The movement the box can actually make, as x, y, z.
///
/// Answers what is left of the movement after the blocks in the way take their share.
[[nodiscard]] std::array<double, 3> collide(const BlockAt& blockAt, const Aabb& box, double dx, double dy, double dz,
                                            const ShapeFor& shapeFor = {});

/// The same move, but allowed to climb up to `stepHeight` on the way.
///
/// parity: `Entity.collide`'s step-up half: when a move is stopped sideways and the thing is on the
/// ground, vanilla retries it lifted by each height a collider offers, in order, and takes the
/// first that gets further than the flat try did. That is what walks a mob onto a slab, and what
/// refuses to walk it onto a whole block.
[[nodiscard]] std::array<double, 3> collideWithStep(const BlockAt& blockAt, const Aabb& box, double dx, double dy,
                                                    double dz, double stepHeight, bool onGround);

/// Whether anything solid overlaps the box. parity: `Level.noCollision`, inverted.
[[nodiscard]] bool collides(const BlockAt& blockAt, const Aabb& box);

/// The blocks whose collision shape overlaps the box, as boxes in world space.
[[nodiscard]] std::vector<Aabb> collidingBoxes(const BlockAt& blockAt, const Aabb& box, const ShapeFor& shapeFor = {});

} // namespace entityCollision

} // namespace basalt::server::runtime::entity
