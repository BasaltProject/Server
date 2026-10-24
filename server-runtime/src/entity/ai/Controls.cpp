#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/CreakingNavigation.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/FlyingPathFinder.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>
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

[[nodiscard]] std::optional<std::vector<std::array<std::int32_t, 3>>>
pathFor(world::WorldState& world, const ServerEntity& mob, double x, double y, double z) {
    // parity: `AmphibiousPathNavigation`.
    if (const std::string_view path = mob.type().path(); path == "frog" || path == "axolotl") {
        const double range = mob.attributeWithModifiers("minecraft:follow_range");
        const std::array<std::int32_t, 3> start =
            pathFinder::amphibiousStartOf(world, mob.x, mob.y, mob.z, mob.width(), mob.onGround, inWater(world, mob));
        const std::array<std::array<std::int32_t, 3>, 1> targets{{{blockOf(x), blockOf(y + 0.5), blockOf(z)}}};
        const std::optional<pathFinder::Route> route =
            pathFinder::findRoute(world, start[0], start[1], start[2], targets,
                                  pathFinder::RouteOptions{.accuracy = 1,
                                                           .maxPathLength = static_cast<float>(range),
                                                           .maxVisited = static_cast<std::int32_t>(range * 16.0),
                                                           .amphibious = true});
        if (!route.has_value()) {
            return std::nullopt;
        }
        return route->nodes;
    }
    if (swimsSmoothly(mob)) {
        // parity: `SwimNodeEvaluator.getStart`, the floor of the box's corner half a block up.
        const double half = mob.width() / 2.0;
        return swimPath::find(world, {blockOf(mob.x - half), blockOf(mob.y + 0.5), blockOf(mob.z - half)},
                              {blockOf(x), blockOf(y), blockOf(z)}, true);
    }
    // parity: `FlyingPathNavigation`, a path through the air from half a block over the feet.
    if (flyingMovement::usesFlyingPath(mob.type())) {
        const double range = mob.attributeWithModifiers("minecraft:follow_range");
        const flyingPathFinder::Footprint size{.width = static_cast<std::int32_t>(std::floor(mob.width() + 1.0)),
                                               .height = static_cast<std::int32_t>(std::floor(mob.height() + 1.0))};
        const flyingPathFinder::Mob flier{
            .maluses = pathTypes::of(mob.type()),
            .at = std::array<std::int32_t, 3>{blockOf(mob.x), blockOf(mob.y), blockOf(mob.z)}};
        std::vector<std::array<std::int32_t, 3>> path = flyingPathFinder::find(
            world, size, {blockOf(mob.x), blockOf(mob.y + 0.5), blockOf(mob.z)}, {blockOf(x), blockOf(y), blockOf(z)},
            static_cast<std::int32_t>(range * 16.0), range, flier);
        return path;
    }
    // parity: `CopperGolem`'s navigation, forty-eight blocks long and through doors.
    if (mob.copperGolem != nullptr) {
        constexpr float kGolemPathLength = 48.0F;
        const std::array<std::int32_t, 3> start = pathFinder::startOf(world, mob.x, mob.y, mob.z, mob.onGround);
        const std::array<std::array<std::int32_t, 3>, 1> targets{{{blockOf(x), blockOf(y), blockOf(z)}}};
        const std::optional<pathFinder::Route> route = pathFinder::findRoute(
            world, start[0], start[1], start[2], targets,
            pathFinder::RouteOptions{.accuracy = 1,
                                     .maxPathLength = kGolemPathLength,
                                     .maxVisited = static_cast<std::int32_t>(kGolemPathLength * 16.0F),
                                     .openDoors = true});
        if (!route.has_value()) {
            return std::nullopt;
        }
        return route->nodes;
    }
    if (mob.navigationOpensDoors.value_or(false)) {
        const double range = mob.attributeWithModifiers("minecraft:follow_range");
        const std::array<std::int32_t, 3> start = pathFinder::startOf(world, mob.x, mob.y, mob.z, mob.onGround);
        const std::array<std::array<std::int32_t, 3>, 1> targets{{{blockOf(x), blockOf(y), blockOf(z)}}};
        const std::optional<pathFinder::Route> route =
            pathFinder::findRoute(world, start[0], start[1], start[2], targets,
                                  pathFinder::RouteOptions{.accuracy = 1,
                                                           .maxPathLength = static_cast<float>(range),
                                                           .maxVisited = static_cast<std::int32_t>(range * 16.0),
                                                           .openDoors = true});
        if (!route.has_value()) {
            return std::nullopt;
        }
        return route->nodes;
    }
    return pathFinder::find(world, blockOf(mob.x), blockOf(mob.y), blockOf(mob.z), blockOf(x), blockOf(y), blockOf(z));
}

} // namespace

namespace navigation {
namespace {

/// parity: `PathNavigation.maxDistanceToWaypoint`, which is the mob's own width or 0.75.
[[nodiscard]] double waypointRange(const ServerEntity& mob) {
    const double width = mob.width();
    return width > 0.75 ? width / 2.0 : 0.75 - width / 2.0;
}

/// parity: `PathNavigation.resetStuckTimeout`.
void resetStuckTimeout(ServerEntity& mob) {
    mob.navigationWatch.timeoutNode = {};
    mob.navigationWatch.timeoutTimer = 0;
    mob.navigationWatch.timeoutLimit = 0.0;
    mob.navigationWatch.stuck = false;
}

/// parity: `moveTo(Path, speed)`.
bool start(ServerEntity& mob, std::vector<std::array<std::int32_t, 3>> nodes, const std::array<double, 3>& target,
           double speedModifier) {
    if (nodes.empty()) {
        return false;
    }
    resetStuckTimeout(mob);
    mob.path = std::move(nodes);
    mob.pathIndex = 0;
    mob.walkTarget = target;
    mob.moveSpeed = speedModifier;
    mob.navigationWatch.lastStuckCheck = mob.navigationWatch.tick;
    mob.navigationWatch.lastStuckCheckPos = {mob.x, mob.y, mob.z};
    return true;
}

} // namespace

bool moveTo(world::WorldState& world, ServerEntity& mob, double x, double y, double z, double speedModifier,
            std::int32_t reachRange) {
    // parity: `Creaking.CreakingPathNavigation`, a navigation of its own.
    if (mob.creaking != nullptr) {
        return creakingNavigation::moveTo(world, mob, x, y, z, reachRange, speedModifier);
    }
    std::optional<std::vector<std::array<std::int32_t, 3>>> path = pathFor(world, mob, x, y, z);
    if (!path.has_value() || path->empty()) {
        return false;
    }
    return start(mob, std::move(*path), {x, y, z}, speedModifier);
}

bool canReach(world::WorldState& world, ServerEntity& mob, double x, double y, double z) {
    const std::optional<std::vector<std::array<std::int32_t, 3>>> path = pathFor(world, mob, x, y, z);
    return path.has_value() && !path->empty();
}

bool follow(ServerEntity& mob, std::vector<std::array<std::int32_t, 3>> nodes, const std::array<double, 3>& target,
            double speedModifier) {
    return start(mob, std::move(nodes), target, speedModifier);
}

bool isDone(const ServerEntity& mob) {
    return !mob.path.has_value() || mob.pathIndex >= static_cast<std::int32_t>(mob.path->size());
}

void stop(ServerEntity& mob) {
    mob.path.reset();
    mob.walkTarget.reset();
    mob.pathIndex = 0;
}

namespace {

/// parity: `PathNavigation.shouldTargetNextNodeInDirection` without a direct line.
[[nodiscard]] bool shouldTargetNextNode(const ServerEntity& mob) {
    const auto next = static_cast<std::size_t>(mob.pathIndex) + 1;
    if (next >= mob.path->size()) {
        return false;
    }
    const std::array<std::int32_t, 3>& current = (*mob.path)[static_cast<std::size_t>(mob.pathIndex)];
    const std::array<std::int32_t, 3>& after = (*mob.path)[next];
    const double toCurrentX = current[0] + 0.5 - mob.x;
    const double toCurrentY = current[1] - mob.y;
    const double toCurrentZ = current[2] + 0.5 - mob.z;
    const double toCurrent = (toCurrentX * toCurrentX) + (toCurrentY * toCurrentY) + (toCurrentZ * toCurrentZ);
    if (toCurrent >= 4.0) {
        return false;
    }
    const double toNextX = after[0] + 0.5 - mob.x;
    const double toNextY = after[1] - mob.y;
    const double toNextZ = after[2] + 0.5 - mob.z;
    const double toNext = (toNextX * toNextX) + (toNextY * toNextY) + (toNextZ * toNextZ);
    if (toNext >= toCurrent && toCurrent >= 0.5) {
        return false;
    }
    const double dot = (toCurrentX * toNextX) + (toCurrentY * toNextY) + (toCurrentZ * toNextZ);
    return dot / (std::sqrt(toCurrent) * std::sqrt(toNext)) < 0.0;
}

/// parity: `canCutCorner`, which a door node refuses.
[[nodiscard]] bool canCutCorner(world::WorldState& world, const std::array<std::int32_t, 3>& node) {
    const std::optional<std::int32_t> state = world.blockState(node[0], node[1], node[2]);
    return !state.has_value() || !pathFinder::isMobDoor(*state);
}

} // namespace

void detectStuck(ServerEntity& mob, const std::array<double, 3>& position, std::int64_t time) {
    ServerEntity::NavigationWatch& watch = mob.navigationWatch;
    if (watch.tick - watch.lastStuckCheck > 100) {
        const float speed = mob.aiSpeed >= 1.0F ? mob.aiSpeed : mob.aiSpeed * mob.aiSpeed;
        const float threshold = speed * 100.0F * 0.25F;
        const double dx = position[0] - watch.lastStuckCheckPos[0];
        const double dy = position[1] - watch.lastStuckCheckPos[1];
        const double dz = position[2] - watch.lastStuckCheckPos[2];
        watch.stuck = (dx * dx) + (dy * dy) + (dz * dz) < static_cast<double>(threshold * threshold);
        if (watch.stuck) {
            stop(mob);
        }
        watch.lastStuckCheck = watch.tick;
        watch.lastStuckCheckPos = position;
    }
    if (isDone(mob)) {
        return;
    }
    const std::array<std::int32_t, 3>& node = (*mob.path)[static_cast<std::size_t>(mob.pathIndex)];
    if (node == watch.timeoutNode) {
        watch.timeoutTimer += time - watch.lastTimeoutCheck;
    } else {
        watch.timeoutNode = node;
        const double dx = position[0] - (node[0] + 0.5);
        const double dy = position[1] - node[1];
        const double dz = position[2] - (node[2] + 0.5);
        const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        watch.timeoutLimit = mob.aiSpeed > 0.0F ? distance / static_cast<double>(mob.aiSpeed) * 20.0 : 0.0;
    }
    if (watch.timeoutLimit > 0.0 && static_cast<double>(watch.timeoutTimer) > watch.timeoutLimit * 3.0) {
        resetStuckTimeout(mob);
        stop(mob);
    }
    watch.lastTimeoutCheck = time;
}

void tick(world::WorldState& world, ServerEntity& mob) {
    if (mob.creaking != nullptr) {
        creakingNavigation::tick(world, mob);
        return;
    }
    mob.navigationWatch.tick++;
    if (!mob.path.has_value()) {
        return;
    }
    if (isDone(mob)) {
        stop(mob);
        return;
    }
    // parity: `followThePath`: one node a tick at most, then the stuck check.
    const double range = waypointRange(mob);
    // parity: `WaterBoundPathNavigation.getMaxVerticalDistanceToWaypoint`.
    const double vertical = swimsSmoothly(mob) ? 0.5 : 1.0;
    const std::array<std::int32_t, 3>& current = (*mob.path)[static_cast<std::size_t>(mob.pathIndex)];
    const bool close = std::abs(mob.x - (current[0] + 0.5)) < range && std::abs(mob.z - (current[2] + 0.5)) < range &&
                       std::abs(mob.y - current[1]) < vertical;
    if (close || (canCutCorner(world, current) && shouldTargetNextNode(mob))) {
        mob.pathIndex++;
    }
    detectStuck(mob, {mob.x, mob.y, mob.z}, mob.navigationWatch.tick);
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
constexpr float kArrivedFloat = 2.5000003E-7F;

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

void strafe(ServerEntity& mob, double strafe, double forward, double speedModifier) {
    mob.strafing = true;
    mob.strafeRight = strafe;
    mob.strafeForward = forward;
    mob.moveSpeed = speedModifier;
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
    if (swimsSmoothly(mob)) {
        tickSwimming(world, mob, speed);
        return;
    }
    // parity: `LivingEntity.travel` picks the fluid path first.
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
    if (mob.strafing) {
        // parity: `MoveControl.Operation.STRAFE`, `travel` on the raw strafe inputs at its speed.
        mob.strafing = false;
        const double amount = speed * mob.moveSpeed;
        livingMovement::travel(world, mob, mob.strafeForward * amount, mob.strafeRight * amount, amount);
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
    // parity: `Mob.setSpeed` also sets the forward input to the same number.
    const double walk = speed * mob.moveSpeed;
    mob.aiSpeed = static_cast<float>(walk);
    // parity: the jump when the next step is above what the mob can climb or it stands inside a collision box.
    if (dy > livingMovement::kStepHeight && dx * dx + dz * dz < std::max(1.0, mob.width())) {
        livingMovement::jump(world, mob);
    } else if (mob.horizontallyBlocked && mob.onGround) {
        livingMovement::jump(world, mob);
    }
    livingMovement::travel(world, mob, walk, 0.0, walk);
}

void tickSwimming(world::WorldState& world, ServerEntity& mob, double speed) {
    // parity: `SmoothSwimmingMoveControl(mob, 85, 10, 0.02F, 0.1F, true)`, the dolphin's.
    constexpr float kMaxTurnX = 85.0F;
    constexpr float kMaxTurnY = 10.0F;
    constexpr float kInWater = 0.02F;
    constexpr float kOutOfWater = 0.1F;
    constexpr double kLift = 0.005;
    const bool wet = inWater(world, mob);
    if (wet) {
        mob.velocityY += kLift;
    }
    double forward = 0.0;
    double up = 0.0;
    double input = 0.0;
    if (mob.moving && !navigation::isDone(mob)) {
        const double dx = mob.wantedX - mob.x;
        const double dy = mob.wantedY - mob.y;
        const double dz = mob.wantedZ - mob.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) >= static_cast<double>(kArrivedFloat)) {
            const float wanted = static_cast<float>(std::atan2(dz, dx) * kDegrees) - 90.0F;
            mob.yaw = rotlerp(mob.yaw, wanted, kMaxTurnY);
            mob.headYaw = mob.yaw;
            const auto full = static_cast<float>(mob.moveSpeed * speed);
            if (wet) {
                input = static_cast<double>(full * kInWater);
                const double flat = std::sqrt((dx * dx) + (dz * dz));
                if (std::abs(dy) > 1.0e-5 || flat > 1.0e-5) {
                    float pitch = -static_cast<float>(std::atan2(dy, flat) * kDegrees);
                    pitch = std::clamp(wrapDegrees(pitch), -kMaxTurnX, kMaxTurnX);
                    // parity: `MoveControl.rotateTowards`, five degrees a tick.
                    mob.pitch = mob.pitch + std::clamp(wrapDegrees(pitch - mob.pitch), -5.0F, 5.0F);
                }
                // parity: `zza`.
                const float radians = mob.pitch * std::numbers::pi_v<float> / 180.0F;
                forward = static_cast<double>(std::cos(radians) * full);
                up = -static_cast<double>(std::sin(radians) * full);
            } else {
                // parity: `getTurningSpeedFactor`, which slows it while it still has a turn to make.
                const float left = std::abs(wrapDegrees(mob.yaw - wanted));
                const float factor = 1.0F - std::clamp((left - 10.0F) / 50.0F, 0.0F, 1.0F);
                input = static_cast<double>(full * kOutOfWater * factor);
                forward = input;
            }
        }
    }
    mob.moving = false;
    // parity: `LivingEntity.applyInput`, which trims the forward input before the travel.
    forward *= static_cast<double>(0.98F);
    mob.swimForward = forward;
    mob.swimUp = up;
    if (wet) {
        livingMovement::travelSwimming(world, mob, forward, up, input, !mob.targetId.has_value());
    } else {
        livingMovement::travel(world, mob, forward, 0.0, input);
    }
}

} // namespace moveControl

namespace lookControl {
namespace {

/// parity: `LookControl.tick`.
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
    if (swimsSmoothly(mob)) {
        tickSwimming(mob);
        return;
    }
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

void tickSwimming(ServerEntity& mob) {
    // parity: `SmoothSwimmingLookControl`.
    constexpr float kHeadTiltY = 20.0F;
    constexpr float kHeadTiltX = 10.0F;
    constexpr float kLevelRate = 5.0F;
    const auto towards = [](float from, float to, float max) {
        return from + std::clamp(moveControl::wrapDegrees(to - from), -max, max);
    };
    if (mob.lookTicks > 0) {
        mob.lookTicks--;
        const double dx = mob.lookX - mob.x;
        const double dy = mob.lookY - (mob.y + mob.height() * kEyeShare);
        const double dz = mob.lookZ - mob.z;
        const double horizontal = std::sqrt(dx * dx + dz * dz);
        if (std::abs(dz) > 1.0e-5 || std::abs(dx) > 1.0e-5) {
            const auto wantedYaw = static_cast<float>(std::atan2(dz, dx) * kDegrees) - 90.0F;
            mob.headYaw = towards(mob.headYaw, wantedYaw + kHeadTiltY, mob.lookYawSpeed);
        }
        if (std::abs(dy) > 1.0e-5 || horizontal > 1.0e-5) {
            const auto wantedPitch = -static_cast<float>(std::atan2(dy, horizontal) * kDegrees);
            mob.pitch = towards(mob.pitch, wantedPitch + kHeadTiltX, mob.lookPitchSpeed);
        }
        return;
    }
    if (navigation::isDone(mob)) {
        mob.pitch = towards(mob.pitch, 0.0F, kLevelRate);
    }
    mob.headYaw = towards(mob.headYaw, mob.yaw, mob.lookYawSpeed);
}

} // namespace lookControl

bool swimsSmoothly(const ServerEntity& mob) { return mob.type().path() == "dolphin" || mob.smoothSwimming; }

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
