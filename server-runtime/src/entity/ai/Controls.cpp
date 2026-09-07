#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace reg = server::registry;

constexpr double kDegrees = 180.0 / std::numbers::pi;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

} // namespace

namespace navigation {
namespace {

/// parity: `PathNavigation.maxDistanceToWaypoint`, which is the mob's own width or 0.75.
[[nodiscard]] double waypointRange(const ServerEntity& mob) {
    const double width = mob.width();
    return width > 0.75 ? width / 2.0 : 0.75 - width / 2.0;
}

} // namespace

bool moveTo(world::WorldState& world, ServerEntity& mob, double x, double y, double z, double speedModifier) {
    const std::optional<std::vector<std::array<std::int32_t, 3>>> path =
        pathFinder::find(world, blockOf(mob.x), blockOf(mob.y), blockOf(mob.z), blockOf(x), blockOf(y), blockOf(z));
    if (!path.has_value() || path->empty()) {
        return false;
    }
    mob.path = path;
    mob.pathIndex = 0;
    mob.walkTarget = std::array<double, 3>{x, y, z};
    mob.moveSpeed = speedModifier;
    return true;
}

bool canReach(world::WorldState& world, ServerEntity& mob, double x, double y, double z) {
    const std::optional<std::vector<std::array<std::int32_t, 3>>> path =
        pathFinder::find(world, blockOf(mob.x), blockOf(mob.y), blockOf(mob.z), blockOf(x), blockOf(y), blockOf(z));
    return path.has_value() && !path->empty();
}

bool isDone(const ServerEntity& mob) {
    return !mob.path.has_value() || mob.pathIndex >= static_cast<std::int32_t>(mob.path->size());
}

void stop(ServerEntity& mob) {
    mob.path.reset();
    mob.walkTarget.reset();
    mob.pathIndex = 0;
}

void tick(world::WorldState& /*world*/, ServerEntity& mob) {
    if (!mob.path.has_value()) {
        return;
    }
    if (isDone(mob)) {
        stop(mob);
        return;
    }
    const double range = waypointRange(mob);
    while (mob.pathIndex < static_cast<std::int32_t>(mob.path->size())) {
        const std::array<std::int32_t, 3>& node = (*mob.path)[static_cast<std::size_t>(mob.pathIndex)];
        const double dx = std::abs(mob.x - (node[0] + 0.5));
        const double dy = std::abs(mob.y - node[1]);
        const double dz = std::abs(mob.z - (node[2] + 0.5));
        if (dx >= range || dz >= range || dy >= 1.0) {
            break;
        }
        mob.pathIndex++;
    }
    if (isDone(mob)) {
        stop(mob);
        return;
    }
    const std::array<std::int32_t, 3>& node = (*mob.path)[static_cast<std::size_t>(mob.pathIndex)];
    moveControl::setWanted(mob, node[0] + 0.5, static_cast<double>(node[1]), node[2] + 0.5, mob.moveSpeed);
}

} // namespace navigation

namespace moveControl {
namespace {

/// parity: the 2.5e-7 under which vanilla calls it arrived.
constexpr double kArrived = 2.5000003e-7;

/// parity: `rotlerp`'s own ninety degrees a tick.
constexpr float kTurnRate = 90.0F;

} // namespace

void setWanted(ServerEntity& mob, double x, double y, double z, double speedModifier) {
    mob.wantedX = x;
    mob.wantedY = y;
    mob.wantedZ = z;
    mob.moveSpeed = speedModifier;
    mob.moving = true;
}

void setForward(ServerEntity& mob, double speedModifier) {
    mob.moveSpeed = speedModifier;
    mob.movingForward = true;
}

void stop(ServerEntity& mob) {
    mob.moving = false;
    mob.movingForward = false;
}

float wrapDegrees(float degrees) {
    float wrapped = std::fmod(degrees, 360.0F);
    if (wrapped >= 180.0F) {
        wrapped -= 360.0F;
    }
    if (wrapped < -180.0F) {
        wrapped += 360.0F;
    }
    return wrapped;
}

float rotlerp(float from, float to, float max) {
    float difference = wrapDegrees(to - from);
    difference = std::clamp(difference, -max, max);
    return from + difference;
}

void tick(world::WorldState& world, ServerEntity& mob, double speed) {
    // parity: `LivingEntity.travel` picks the fluid path first: a squid pushes itself along in water
    // rather than walking, and nothing in water falls like a stone.
    if (inWater(world, mob)) {
        livingMovement::travelInWater(world, mob, 0.0, 0.0);
        mob.moving = false;
        return;
    }
    if (mob.movingForward) {
        mob.movingForward = false;
        const double ahead = speed * mob.moveSpeed;
        livingMovement::travel(world, mob, ahead, 0.0, ahead);
        return;
    }
    if (!mob.moving) {
        livingMovement::travel(world, mob, 0.0, 0.0, 0.0);
        return;
    }
    mob.moving = false;
    const double dx = mob.wantedX - mob.x;
    const double dy = mob.wantedY - mob.y;
    const double dz = mob.wantedZ - mob.z;
    if (dx * dx + dy * dy + dz * dz < kArrived) {
        livingMovement::travel(world, mob, 0.0, 0.0, 0.0);
        return;
    }
    const auto wanted = static_cast<float>(std::atan2(dz, dx) * kDegrees) - 90.0F;
    mob.yaw = rotlerp(mob.yaw, wanted, kTurnRate);
    // parity: `Mob.setSpeed`, which also sets the forward input to the same number, so what a mob
    // accelerates by is the speed times itself. Passing one here made every mob run.
    const double walk = speed * mob.moveSpeed;
    // parity: the jump when the next step is above what the mob can climb, or when it is stood
    // inside something with a collision box.
    if (dy > livingMovement::kStepHeight && dx * dx + dz * dz < std::max(1.0, mob.width())) {
        livingMovement::jump(world, mob);
    } else if (mob.horizontallyBlocked && mob.onGround) {
        livingMovement::jump(world, mob);
    }
    livingMovement::travel(world, mob, walk, 0.0, walk);
}

} // namespace moveControl

namespace lookControl {
namespace {

/// parity: `LookControl.tick`, which brings the head back to the body ten degrees at a time.
constexpr float kDriftRate = 10.0F;
constexpr double kEyeShare = 0.85;
constexpr float kMaxHeadTurn = 75.0F;

} // namespace

void lookAt(ServerEntity& mob, double x, double y, double z, float maxYaw, float maxPitch) {
    mob.lookX = x;
    mob.lookY = y;
    mob.lookZ = z;
    mob.lookTicks = 2;
    mob.lookYawSpeed = maxYaw;
    mob.lookPitchSpeed = maxPitch;
}

void tick(ServerEntity& mob) {
    if (mob.lookTicks > 0) {
        mob.lookTicks--;
        const double dx = mob.lookX - mob.x;
        const double dy = mob.lookY - (mob.y + mob.height() * kEyeShare);
        const double dz = mob.lookZ - mob.z;
        const double horizontal = std::sqrt(dx * dx + dz * dz);
        const auto wantedYaw = static_cast<float>(std::atan2(dz, dx) * kDegrees) - 90.0F;
        const auto wantedPitch = -static_cast<float>(std::atan2(dy, horizontal) * kDegrees);
        mob.headYaw = moveControl::rotlerp(mob.headYaw, wantedYaw, mob.lookYawSpeed);
        mob.pitch = moveControl::rotlerp(mob.pitch, wantedPitch, mob.lookPitchSpeed);
    } else {
        mob.headYaw = moveControl::rotlerp(mob.headYaw, mob.yaw, kDriftRate);
    }
    // parity: `clampHeadRotationToBody`, which is what stops a walking mob looking backwards.
    if (mob.path.has_value()) {
        mob.headYaw = moveControl::rotlerp(mob.headYaw, mob.yaw, kMaxHeadTurn);
    }
}

} // namespace lookControl

bool inWater(world::WorldState& world, const ServerEntity& mob) {
    const std::optional<std::int32_t> state = world.blockState(blockOf(mob.x), blockOf(mob.y), blockOf(mob.z));
    if (!state.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = reg::blockStateDefinitions::blockOf(*state);
    return block != nullptr && block->path() == "water";
}

bool isPassable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && reg::blockShapes::isEmptyShape(*state);
}

} // namespace basalt::server::runtime::entity::ai
