#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::server::runtime::entity {

/// The things that are thrown or shot: how they fly, and where they land.
///
/// parity: `Projectile.shoot` for the aim, `ThrowableProjectile.tick` and `AbstractArrow.tick` for
/// the flight. The two differ in what they weigh and what stops them: a snowball is gone the moment
/// it hits anything, an arrow sticks in the block it hit and stays there.
namespace projectiles {

/// One kind of projectile: what it weighs, what the air does to it, and whether it sticks.
struct Kind {
    wire::common::Identifier type;
    double gravity = 0.0;
    double drag = 0.0;
    bool sticks = false;
};

/// parity: `ProjectileItem.DispenseConfig`'s own defaults.
inline constexpr float kDispenserPower = 1.1F;
inline constexpr float kDispenserUncertainty = 6.0F;

/// parity: `AbstractArrow.getDefaultGravity` and its 0.99 of air drag.
[[nodiscard]] const Kind& arrow();

/// parity: `AbstractHurtingProjectile`: a fireball does not fall at all, and keeps 0.95 of its speed
/// a tick, which is why one crosses a room in a straight line.
[[nodiscard]] const Kind& fireball();

/// parity: `ThrowableProjectile.getDefaultGravity`, lighter than an arrow and gone on impact.
[[nodiscard]] const Kind& snowball();

/// Every kind there is, which is what a flying entity's type is looked up in.
[[nodiscard]] std::span<const Kind> all();

/// What a dispenser shoots for this item, or nothing when it just throws it.
[[nodiscard]] const Kind* dispensed(const wire::common::Identifier& item);

/// The kind a flying entity of this type is, so a tick knows what it weighs.
[[nodiscard]] const Kind* kindOf(const wire::common::Identifier& type);

/// Puts a projectile in the world, aimed along the direction given.
ServerEntity& spawn(world::WorldState& world, const Kind& kind, std::int32_t entityId, std::int32_t protocolType,
                    double x, double y, double z, double dirX, double dirY, double dirZ, float power, float uncertainty,
                    random::RandomSource& random);

/// parity: `Projectile.shoot`: the direction is normalised, each axis gets its own triangular spread,
/// and the whole thing is scaled by the power. The rotation follows the movement, which is what
/// points an arrow the way it is going.
void shoot(ServerEntity& entity, double dirX, double dirY, double dirZ, float power, float uncertainty,
           random::RandomSource& random);

/// One tick of flight. Answers where it hit, or nothing if it is still going.
///
/// parity: the order both projectile classes share: drag, then the line from where it is to where it
/// would be, then gravity. An arrow that hit stops dead and stays; anything else is the caller's to
/// remove.
[[nodiscard]] std::optional<BlockHit> tick(world::WorldState& world, ServerEntity& entity, const Kind& kind);

/// How far along the line from/to a box is first touched, from zero to one, or nothing when the line
/// misses it.
///
/// parity: `AABB.clip` over `ProjectileUtil.getEntityHitResult`, which is what makes an arrow hit
/// whoever it passes through rather than only whoever it ends the tick inside.
[[nodiscard]] std::optional<double> clipBox(double fromX, double fromY, double fromZ, double toX, double toY,
                                            double toZ, double minX, double minY, double minZ, double maxX, double maxY,
                                            double maxZ);

} // namespace projectiles

} // namespace basalt::server::runtime::entity
