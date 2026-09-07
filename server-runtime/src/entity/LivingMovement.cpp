#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace basalt::server::runtime::entity::livingMovement {
namespace {

/// parity: the 0.91 of air drag and the 0.98 the vertical keeps.
constexpr double kAirDrag = 0.91;
constexpr double kVerticalDrag = 0.98;

/// parity: `travelInWater`: a fiftieth of a block of input, and 0.8 of the speed kept.
constexpr double kWaterInput = 0.02;
constexpr double kWaterDrag = 0.8;
constexpr double kVerticalWaterDrag = 0.8;
constexpr double kSinkShare = 16.0;

/// parity: `getFrictionInfluencedSpeed`'s own constant, which is 0.6 cubed to seven decimals. The
/// two branches below therefore agree at the default friction, and only slippery ground diverges.
constexpr double kFrictionSpeed = 0.21600002;

/// parity: `getFlyingSpeed`, what steering in mid-air is worth.
constexpr double kFlyingSpeed = 0.02;

/// parity: `getBlockPosBelowThatAffectsMyMovement`, which looks half a block under the feet.
constexpr double kFrictionProbeDepth = 0.5;

/// parity: `Mth.equal`, the slack every collision comparison in vanilla is made with.
constexpr double kEqualEpsilon = 1.0e-5;

/// parity: the hundred-thousandth of slack `Entity.move` compares with. Comparing exactly called a
/// stopped rounding error a wall, so every mob hopped its way across flat ground and spent its life
/// in the air at flying speed.
[[nodiscard]] bool nearlyEqual(double one, double other) { return std::abs(other - one) < kEqualEpsilon; }

[[nodiscard]] std::int32_t blockUnder(world::WorldState& world, const ServerEntity& entity) {
    return world
        .blockState(static_cast<std::int32_t>(std::floor(entity.x)),
                    static_cast<std::int32_t>(std::floor(entity.y - kFrictionProbeDepth)),
                    static_cast<std::int32_t>(std::floor(entity.z)))
        .value_or(0);
}

[[nodiscard]] entityCollision::BlockAt blocksOf(world::WorldState& world) {
    return [&world](std::int32_t x, std::int32_t y, std::int32_t z) { return world.blockState(x, y, z); };
}

/// parity: `getFrictionInfluencedSpeed`: slippery ground asks for more, so the drag cancels less.
[[nodiscard]] double frictionInfluencedSpeed(const ServerEntity& entity, double speed, double blockFriction) {
    if (!entity.onGround) {
        return kFlyingSpeed;
    }
    if (blockFriction > static_cast<double>(registry::blockMovement::kDefaultFriction)) {
        return speed * (kFrictionSpeed / (blockFriction * blockFriction * blockFriction));
    }
    return speed;
}

/// parity: `Entity.getInputVector`, which turns the input into world axes through the yaw.
void addInput(ServerEntity& entity, double forward, double strafe, double speed) {
    const double length = (forward * forward) + (strafe * strafe);
    if (length < 1.0e-7) {
        return;
    }
    const double scale = length > 1.0 ? speed / std::sqrt(length) : speed;
    const double x = strafe * scale;
    const double z = forward * scale;
    const double radians = static_cast<double>(entity.yaw) * std::numbers::pi / 180.0;
    const double sinYaw = std::sin(radians);
    const double cosYaw = std::cos(radians);
    entity.velocityX += (x * cosYaw) - (z * sinYaw);
    entity.velocityZ += (z * cosYaw) + (x * sinYaw);
}

} // namespace

void travel(world::WorldState& world, ServerEntity& entity, double forward, double strafe, double speed,
            double gravity) {
    const double blockFriction =
        entity.onGround ? static_cast<double>(registry::blockMovement::frictionOf(blockUnder(world, entity))) : 1.0;
    addInput(entity, forward, strafe, frictionInfluencedSpeed(entity, speed, blockFriction));

    const Aabb box = Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
    const std::array<double, 3> moved = entityCollision::collideWithStep(
        blocksOf(world), box, entity.velocityX, entity.velocityY, entity.velocityZ, kStepHeight, entity.onGround);
    entity.x += moved[0];
    entity.y += moved[1];
    entity.z += moved[2];
    const bool collidedX = !nearlyEqual(moved[0], entity.velocityX);
    const bool collidedZ = !nearlyEqual(moved[2], entity.velocityZ);
    const bool collidedY = moved[1] != entity.velocityY;
    entity.onGround = collidedY && entity.velocityY < 0.0;
    const bool blockedSideways = collidedX || collidedZ;
    if (collidedX) {
        entity.velocityX = 0.0;
    }
    if (collidedZ) {
        entity.velocityZ = 0.0;
    }
    if (collidedY) {
        entity.velocityY = 0.0;
    }

    // parity: gravity comes off after the move, and the drag after that.
    entity.velocityY -= gravity;
    const double horizontalDrag = blockFriction * kAirDrag;
    entity.velocityX *= horizontalDrag;
    entity.velocityY *= kVerticalDrag;
    entity.velocityZ *= horizontalDrag;
    entity.horizontallyBlocked = blockedSideways;
}

void travelInWater(world::WorldState& world, ServerEntity& entity, double forward, double strafe) {
    addInput(entity, forward, strafe, kWaterInput);

    const Aabb box = Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
    const std::array<double, 3> moved =
        entityCollision::collide(blocksOf(world), box, entity.velocityX, entity.velocityY, entity.velocityZ);
    entity.x += moved[0];
    entity.y += moved[1];
    entity.z += moved[2];
    entity.onGround = moved[1] != entity.velocityY && entity.velocityY < 0.0;
    const bool collidedX = !nearlyEqual(moved[0], entity.velocityX);
    const bool collidedZ = !nearlyEqual(moved[2], entity.velocityZ);
    entity.horizontallyBlocked = collidedX || collidedZ;
    if (collidedX) {
        entity.velocityX = 0.0;
    }
    if (collidedZ) {
        entity.velocityZ = 0.0;
    }
    if (moved[1] != entity.velocityY) {
        entity.velocityY = 0.0;
    }

    entity.velocityX *= kWaterDrag;
    entity.velocityZ *= kWaterDrag;
    // parity: `getFluidFallingAdjustedMovement`, a sixteenth of the gravity while sinking.
    entity.velocityY = (entity.velocityY * kVerticalWaterDrag) - (kGravity / kSinkShare);
}

void jump(world::WorldState& world, ServerEntity& entity) {
    if (!entity.onGround) {
        return;
    }
    entity.velocityY =
        kJumpPower * static_cast<double>(registry::blockMovement::jumpFactorOf(blockUnder(world, entity)));
    entity.onGround = false;
}

} // namespace basalt::server::runtime::entity::livingMovement
