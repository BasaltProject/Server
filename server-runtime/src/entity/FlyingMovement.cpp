#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

namespace basalt::server::runtime::entity {
namespace {

namespace mth = wire::common::mth;

constexpr double kDegrees = 180.0 / std::numbers::pi;
constexpr float kRadians = std::numbers::pi_v<float> / 180.0F;
/// parity: `MoveControl.MIN_SPEED_SQR`.
constexpr double kArrived = static_cast<double>(2.5000003E-7F);
constexpr double kGravity = 0.08;
/// parity: `LivingEntity.aiStep`, which zeroes a mob's speed under three thousandths an axis.
constexpr double kStill = 0.003;

/// parity: `Mth.wrapDegrees(float)`.
[[nodiscard]] float wrapDegrees(float degrees) {
    float wrapped = std::fmod(degrees, 360.0F);
    if (wrapped >= 180.0F) {
        wrapped -= 360.0F;
    }
    if (wrapped < -180.0F) {
        wrapped += 360.0F;
    }
    return wrapped;
}

/// parity: `Mth.approach`.
[[nodiscard]] float approach(float value, float target, float step) {
    step = std::abs(step);
    return value < target ? std::clamp(value + step, value, target) : std::clamp(value - step, target, value);
}

/// parity: `Mth.approachDegrees`.
[[nodiscard]] float approachDegrees(float from, float to, float step) {
    return approach(from, from + wrapDegrees(to - from), step);
}

/// parity: `MoveControl.rotlerp`, clamped and then brought back into 0..360.
[[nodiscard]] float rotlerp(float from, float to, float max) {
    const float difference = std::clamp(wrapDegrees(to - from), -max, max);
    float result = from + difference;
    if (result < 0.0F) {
        result += 360.0F;
    } else if (result > 360.0F) {
        result -= 360.0F;
    }
    return result;
}

[[nodiscard]] world::Submersion submersionOf(world::WorldState& world, const ServerEntity& mob) {
    const std::optional<std::int32_t> state = world.blockState(mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z));
    return state.has_value() ? world::fluids::of(*state) : world::Submersion::None;
}

void settle(ServerEntity& mob) {
    if (std::abs(mob.velocityX) < kStill) {
        mob.velocityX = 0.0;
    }
    if (std::abs(mob.velocityY) < kStill) {
        mob.velocityY = 0.0;
    }
    if (std::abs(mob.velocityZ) < kStill) {
        mob.velocityZ = 0.0;
    }
}

/// parity: `FlyingMoveControl(mob, maxTurn, hoversInPlace)` and `Mob.travel` over `travelInAir`.
void flyingControl(world::WorldState& world, ServerEntity& mob, float maxTurn, bool hovers, bool omnidirectional) {
    livingMovement::Input input;
    double speed = 0.0;
    if (mob.moving) {
        mob.moving = false;
        mob.noGravity = true;
        const double dx = mob.wantedX - mob.x;
        const double dy = mob.wantedY - mob.y;
        const double dz = mob.wantedZ - mob.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) >= kArrived) {
            const auto yaw = static_cast<float>(std::atan2(dz, dx) * kDegrees) - 90.0F;
            mob.yaw = rotlerp(mob.yaw, yaw, 90.0F);
            const double attribute =
                mob.attributeWithModifiers(mob.onGround ? "minecraft:movement_speed" : "minecraft:flying_speed");
            speed = static_cast<double>(static_cast<float>(mob.moveSpeed * attribute));
            input.forward = speed;
            const double flat = std::sqrt((dx * dx) + (dz * dz));
            if (std::abs(dy) > static_cast<double>(1.0E-5F) || std::abs(flat) > static_cast<double>(1.0E-5F)) {
                const auto pitch = static_cast<float>(-(std::atan2(dy, flat) * kDegrees));
                mob.pitch = rotlerp(mob.pitch, pitch, maxTurn);
                input.up = dy > 0.0 ? speed : -speed;
            }
        }
    } else if (!hovers) {
        mob.noGravity = false;
    }
    livingMovement::travelAir(world, mob, input, speed, mob.noGravity ? 0.0 : kGravity, omnidirectional);
}

/// parity: `GhastMoveControl.tick` and `Ghast.travel`.
void ghastControl(world::WorldState& world, ServerEntity& mob, random::RandomSource& random) {
    if (mob.moving && mob.flight.floatDuration-- <= 0) {
        mob.flight.floatDuration += random.nextInt(5) + 2;
        const double dx = mob.wantedX - mob.x;
        const double dy = mob.wantedY - mob.y;
        const double dz = mob.wantedZ - mob.z;
        if (flyingMovement::ghastCanReach(world, mob, dx, dy, dz)) {
            const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
            const double push = mob.attributeWithModifiers("minecraft:flying_speed") * 5.0 / 3.0;
            if (length >= 1.0E-5) {
                mob.velocityX += dx / length * push;
                mob.velocityY += dy / length * push;
                mob.velocityZ += dz / length * push;
            }
        } else {
            mob.moving = false;
        }
    }
    livingMovement::travelFlying(world, mob, {}, 0.02F, submersionOf(world, mob));
}

/// parity: `Vex.VexMoveControl.tick`, then a travel with no physics and no gravity.
void vexControl(world::WorldState& world, ServerEntity& mob, const std::optional<std::array<double, 3>>& target) {
    mob.noGravity = true;
    if (mob.moving) {
        const double dx = mob.wantedX - mob.x;
        const double dy = mob.wantedY - mob.y;
        const double dz = mob.wantedZ - mob.z;
        const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        // parity: `AABB.getSize`, the mean of the three sides.
        const double size = ((mob.width() * 2.0) + mob.height()) / 3.0;
        if (length < size) {
            mob.moving = false;
            mob.velocityX *= 0.5;
            mob.velocityY *= 0.5;
            mob.velocityZ *= 0.5;
        } else {
            const double scale = mob.moveSpeed * 0.05 / length;
            mob.velocityX += dx * scale;
            mob.velocityY += dy * scale;
            mob.velocityZ += dz * scale;
            if (target.has_value()) {
                mob.yaw = -static_cast<float>(std::atan2((*target)[0] - mob.x, (*target)[2] - mob.z)) *
                          (180.0F / std::numbers::pi_v<float>);
            } else {
                mob.yaw = -static_cast<float>(std::atan2(mob.velocityX, mob.velocityZ)) *
                          (180.0F / std::numbers::pi_v<float>);
            }
            mob.headYaw = mob.yaw;
        }
    }
    livingMovement::travelAir(world, mob, {}, 0.0, 0.0, false, true);
}

/// parity: `Phantom.PhantomMoveControl.tick` and `Phantom.travel`.
void phantomControl(world::WorldState& world, ServerEntity& mob) {
    ServerEntity::Flight& flight = mob.flight;
    if (mob.horizontallyBlocked) {
        mob.yaw += 180.0F;
        flight.phantomSpeed = 0.1F;
    }
    double tdx = flight.moveTargetPoint[0] - mob.x;
    const double tdy = flight.moveTargetPoint[1] - mob.y;
    double tdz = flight.moveTargetPoint[2] - mob.z;
    double flat = std::sqrt((tdx * tdx) + (tdz * tdz));
    if (std::abs(flat) > static_cast<double>(1.0E-5F)) {
        const double shrink = 1.0 - (std::abs(tdy * static_cast<double>(0.7F)) / flat);
        tdx *= shrink;
        tdz *= shrink;
        flat = std::sqrt((tdx * tdx) + (tdz * tdz));
        const double full = std::sqrt((tdx * tdx) + (tdz * tdz) + (tdy * tdy));
        const float before = mob.yaw;
        const auto angle = static_cast<float>(std::atan2(tdz, tdx));
        const float from = wrapDegrees(mob.yaw + 90.0F);
        const float to = wrapDegrees(angle * (180.0F / std::numbers::pi_v<float>));
        mob.yaw = approachDegrees(from, to, 4.0F) - 90.0F;
        mob.headYaw = mob.yaw;
        if (std::abs(wrapDegrees(mob.yaw - before)) < 3.0F) {
            flight.phantomSpeed = approach(flight.phantomSpeed, 1.8F, 0.005F * (1.8F / flight.phantomSpeed));
        } else {
            flight.phantomSpeed = approach(flight.phantomSpeed, 0.2F, 0.025F);
        }
        const auto pitch = static_cast<float>(-(std::atan2(-tdy, flat) * kDegrees));
        mob.pitch = pitch;
        const float moveAngle = mob.yaw + 90.0F;
        const double speed = static_cast<double>(flight.phantomSpeed);
        const double tx =
            static_cast<double>(static_cast<float>(speed) * mth::cos(moveAngle * kRadians)) * std::abs(tdx / full);
        const double tz =
            static_cast<double>(static_cast<float>(speed) * mth::sin(moveAngle * kRadians)) * std::abs(tdz / full);
        const double ty =
            static_cast<double>(static_cast<float>(speed) * mth::sin(pitch * kRadians)) * std::abs(tdy / full);
        mob.velocityX += (tx - mob.velocityX) * 0.2;
        mob.velocityY += (ty - mob.velocityY) * 0.2;
        mob.velocityZ += (tz - mob.velocityZ) * 0.2;
    }
    livingMovement::travelFlying(world, mob, {}, 0.2F, submersionOf(world, mob));
}

/// parity: `Bat`'s travel with the half forward its AI step asks for, and `Bat.tick` after it.
void batControl(world::WorldState& world, ServerEntity& mob) {
    if (mob.flight.resting) {
        mob.velocityX = 0.0;
        mob.velocityY = 0.0;
        mob.velocityZ = 0.0;
        mob.y = std::floor(mob.y) + 1.0 - mob.height();
        return;
    }
    livingMovement::travelAir(world, mob, {.forward = 0.5}, 0.0, mob.noGravity ? 0.0 : kGravity, false);
    mob.velocityY *= 0.6;
}

} // namespace

namespace flyingMovement {

Control controlOf(const wire::common::Identifier& type) {
    const std::string_view path = type.path();
    if (path == "wither" || path == "parrot" || path == "bee" || path == "allay") {
        return Control::Flying;
    }
    if (path == "ghast") {
        return Control::Ghast;
    }
    if (path == "vex") {
        return Control::Vex;
    }
    if (path == "phantom") {
        return Control::Phantom;
    }
    return path == "bat" ? Control::Bat : Control::Drift;
}

bool usesFlyingPath(const wire::common::Identifier& type) { return controlOf(type) == Control::Flying; }

bool ghastCanReach(world::WorldState& world, const ServerEntity& mob, double dx, double dy, double dz) {
    const Aabb from = Aabb::of(mob.x, mob.y, mob.z, mob.width(), mob.height());
    const Aabb to = from.move(dx, dy, dz);
    const Aabb swept{std::min(from.minX, to.minX), std::min(from.minY, to.minY), std::min(from.minZ, to.minZ),
                     std::max(from.maxX, to.maxX), std::max(from.maxY, to.maxY), std::max(from.maxZ, to.maxZ)};
    const entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    const double halfX = ((from.maxX - from.minX) * 0.5) - 1.0E-7;
    const double halfY = ((from.maxY - from.minY) * 0.5) - 1.0E-7;
    const double halfZ = ((from.maxZ - from.minZ) * 0.5) - 1.0E-7;
    const double cx = (from.minX + from.maxX) * 0.5;
    const double cy = (from.minY + from.maxY) * 0.5;
    const double cz = (from.minZ + from.maxZ) * 0.5;
    // parity: every block the box passes through, each against its own shape; the cells are walked
    // here rather than gathered, since a ghast looks up to sixty blocks ahead.
    for (std::int32_t y = mth::floor(swept.minY); y <= mth::floor(swept.maxY); ++y) {
        for (std::int32_t z = mth::floor(swept.minZ); z <= mth::floor(swept.maxZ); ++z) {
            for (std::int32_t x = mth::floor(swept.minX); x <= mth::floor(swept.maxX); ++x) {
                const std::optional<std::int32_t> state = world.blockState(x, y, z);
                if (!state.has_value() || registry::blockShapes::isEmptyShape(*state)) {
                    continue;
                }
                const Aabb cell{
                    static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), x + 1.0, y + 1.0, z + 1.0};
                // parity: a block the box already overlaps is let through, whatever is in it.
                if (from.intersects(cell)) {
                    continue;
                }
                for (const Aabb& shape : entityCollision::collidingBoxes(blockAt, cell.inflate(-1.0E-7))) {
                    // parity: `AABB.collidedAlongVector`, the centre's line through the shape grown
                    // by half the box.
                    const Aabb grown{shape.minX - halfX, shape.minY - halfY, shape.minZ - halfZ,
                                     shape.maxX + halfX, shape.maxY + halfY, shape.maxZ + halfZ};
                    if (grown.contains(cx + dx, cy + dy, cz + dz) || grown.contains(cx, cy, cz) ||
                        projectiles::clipBox(cx, cy, cz, cx + dx, cy + dy, cz + dz, grown.minX, grown.minY, grown.minZ,
                                             grown.maxX, grown.maxY, grown.maxZ)
                            .has_value()) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

void tick(world::WorldState& world, ServerEntity& entity, random::RandomSource& random,
          const std::optional<std::array<double, 3>>& target) {
    settle(entity);
    switch (controlOf(entity.type())) {
    case Control::Flying: {
        const std::string_view path = entity.type().path();
        const bool small = path == "bee" || path == "allay";
        flyingControl(world, entity, small ? 20.0F : 10.0F, small, path == "bee" || path == "parrot");
        break;
    }
    case Control::Ghast:
        ghastControl(world, entity, random);
        break;
    case Control::Vex:
        vexControl(world, entity, target);
        break;
    case Control::Phantom:
        phantomControl(world, entity);
        break;
    case Control::Bat:
        batControl(world, entity);
        break;
    case Control::Drift:
        livingMovement::travelFlying(world, entity, {}, 0.02F, submersionOf(world, entity));
        break;
    }
}

} // namespace flyingMovement

namespace flyers {
namespace {

constexpr auto kFlying =
    std::to_array<std::string_view>({"ghast", "happy_ghast", "ghastling", "phantom", "vex", "bat", "allay", "wither",
                                     "ender_dragon", "breeze", "parrot", "bee"});

} // namespace

bool flies(const wire::common::Identifier& type) { return std::ranges::find(kFlying, type.path()) != kFlying.end(); }

} // namespace flyers

} // namespace basalt::server::runtime::entity
