#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace basalt::server::runtime::entity::mobBrain {
namespace {

namespace reg = server::registry;

/// parity: `DefaultRandomPos.generateRandomPos`, which tries ten and keeps the best.
constexpr std::int32_t kCandidates = 10;

/// How close counts as arrived. parity: `PathNavigation`'s own tolerance, one block.
constexpr double kArrived = 1.0;

/// parity: `MoveControl.tick`'s turn rate, thirty degrees a tick.
constexpr float kTurnRate = 30.0F;

/// parity: how far vanilla lifts a spot out of a hill, and how far a mob will step down to one.
constexpr std::int32_t kLiftLimit = 6;
constexpr std::int32_t kDropLimit = 4;

[[nodiscard]] std::int32_t blockOf(double coordinate) { return static_cast<std::int32_t>(std::floor(coordinate)); }

[[nodiscard]] bool isPassable(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && reg::blockShapes::isEmptyShape(*state);
}

/// The spot a mob would stand on at these coordinates: out of the ground if it is inside it, and
/// down onto it if it is over it.
///
/// parity: `RandomPos.moveUpOutOfSolid` and the drop `WalkNodeEvaluator` allows underneath.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> groundAt(world::WorldState& world, std::int32_t x,
                                                                  std::int32_t y, std::int32_t z) {
    std::int32_t top = y;
    while (top <= y + kLiftLimit && !isPassable(world, x, top, z)) {
        top++;
    }
    if (top > y + kLiftLimit) {
        return std::nullopt;
    }
    std::int32_t standing = top;
    std::int32_t dropped = 0;
    while (dropped < kDropLimit && isPassable(world, x, standing - 1, z)) {
        standing--;
        dropped++;
    }
    const std::optional<std::int32_t> below = world.blockState(x, standing - 1, z);
    if (!below.has_value() || reg::blockShapes::isEmptyShape(*below)) {
        return std::nullopt;
    }
    return std::array<std::int32_t, 3>{x, standing, z};
}

/// parity: `PathfinderMob.getWalkTargetValue`, which is zero for anything that will do; here it
/// simply refuses what cannot be stood on, which is what makes a wander stay on the ground.
[[nodiscard]] double walkTargetValue(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> ground = world.blockState(x, y - 1, z);
    if (!ground.has_value() || reg::blockShapes::isEmptyShape(*ground)) {
        return -std::numeric_limits<double>::infinity();
    }
    const std::optional<std::int32_t> feet = world.blockState(x, y, z);
    const std::optional<std::int32_t> head = world.blockState(x, y + 1, z);
    if (!feet.has_value() || !head.has_value() || !reg::blockShapes::isEmptyShape(*feet) ||
        !reg::blockShapes::isEmptyShape(*head)) {
        return -std::numeric_limits<double>::infinity();
    }
    return 0.0;
}

[[nodiscard]] bool farFrom(const ServerEntity& mob, const std::array<double, 3>& point) {
    const double dx = point[0] - mob.x;
    const double dz = point[2] - mob.z;
    return dx * dx + dz * dz >= kArrived * kArrived;
}

/// The node it is walking to now, stepping on when it arrives. Nothing when the path is walked out.
[[nodiscard]] std::optional<std::array<double, 3>> nextNode(ServerEntity& mob) {
    if (!mob.path.has_value()) {
        if (mob.walkTarget.has_value() && farFrom(mob, *mob.walkTarget)) {
            return mob.walkTarget;
        }
        return std::nullopt;
    }
    while (mob.pathIndex < static_cast<std::int32_t>(mob.path->size())) {
        const std::array<std::int32_t, 3>& node = (*mob.path)[static_cast<std::size_t>(mob.pathIndex)];
        const std::array<double, 3> centre{node[0] + 0.5, static_cast<double>(node[1]), node[2] + 0.5};
        if (farFrom(mob, centre)) {
            return centre;
        }
        mob.pathIndex++;
    }
    return std::nullopt;
}

/// parity: `Mth.approachDegrees`, the shortest way round, capped at `limit`.
[[nodiscard]] float approachAngle(float from, float to, float limit) {
    float difference = std::fmod(to - from, 360.0F);
    if (difference < -180.0F) {
        difference += 360.0F;
    }
    if (difference > 180.0F) {
        difference -= 360.0F;
    }
    return from + std::clamp(difference, -limit, limit);
}

/// parity: `MoveControl.tick`, which turns at most thirty degrees a tick towards the target.
void turnTowards(ServerEntity& mob, double dx, double dz) {
    const auto wanted = static_cast<float>(std::atan2(dz, dx) * 180.0 / std::numbers::pi) - 90.0F;
    mob.yaw = approachAngle(mob.yaw, wanted, kTurnRate);
}

} // namespace

bool pathTo(world::WorldState& world, ServerEntity& mob, double x, double y, double z) {
    const std::optional<std::vector<std::array<std::int32_t, 3>>> path =
        pathFinder::find(world, blockOf(mob.x), blockOf(mob.y), blockOf(mob.z), blockOf(x), blockOf(y), blockOf(z));
    if (!path.has_value() || path->empty()) {
        return false;
    }
    mob.walkTarget = std::array<double, 3>{x, y, z};
    mob.path = path;
    mob.pathIndex = 0;
    return true;
}

std::optional<std::array<double, 3>> strollTarget(world::WorldState& world, const ServerEntity& mob,
                                                  random::RandomSource& random, std::int32_t range,
                                                  std::int32_t height) {
    std::optional<std::array<double, 3>> best;
    double bestWeight = -std::numeric_limits<double>::infinity();
    for (std::int32_t attempt = 0; attempt < kCandidates; attempt++) {
        const std::int32_t x = blockOf(mob.x) + random.nextInt(2 * range + 1) - range;
        const std::int32_t y = blockOf(mob.y) + random.nextInt(2 * height + 1) - height;
        const std::int32_t z = blockOf(mob.z) + random.nextInt(2 * range + 1) - range;
        // parity: `LandRandomPos.movePosUpOutOfSolid`, which lifts a spot out of the ground rather
        // than throwing it away: on real terrain nearly every raw guess is inside a hill.
        const std::optional<std::array<std::int32_t, 3>> standing = groundAt(world, x, y, z);
        if (!standing.has_value()) {
            continue;
        }
        const double weight = walkTargetValue(world, (*standing)[0], (*standing)[1], (*standing)[2]);
        if (weight <= bestWeight) {
            continue;
        }
        bestWeight = weight;
        best = std::array<double, 3>{(*standing)[0] + 0.5, static_cast<double>((*standing)[1]), (*standing)[2] + 0.5};
    }
    return best;
}

bool walk(world::WorldState& world, ServerEntity& mob, double speed, double modifier) {
    const std::optional<std::array<double, 3>> node = nextNode(mob);
    if (!node.has_value()) {
        mob.walkTarget.reset();
        mob.path.reset();
        livingMovement::travel(world, mob, 0.0, 0.0, speed);
        return false;
    }
    turnTowards(mob, (*node)[0] - mob.x, (*node)[2] - mob.z);
    livingMovement::travel(world, mob, 1.0, 0.0, speed * modifier);
    // parity: `MoveControl`'s own jump when the way is blocked but the step was not enough.
    if (mob.horizontallyBlocked && mob.onGround) {
        livingMovement::jump(world, mob);
    }
    return true;
}

void tick(world::WorldState& world, ServerEntity& mob, random::RandomSource& random, double speed, double modifier) {
    mob.age++;
    if (!mob.walkTarget.has_value() && random.nextInt(kStrollInterval) == 0) {
        if (const std::optional<std::array<double, 3>> target = strollTarget(world, mob, random); target.has_value()) {
            static_cast<void>(pathTo(world, mob, (*target)[0], (*target)[1], (*target)[2]));
        }
    }
    static_cast<void>(walk(world, mob, speed, modifier));
}

double distanceTo(const ServerEntity& mob, double x, double y, double z) {
    const double dx = mob.x - x;
    const double dy = mob.y - y;
    const double dz = mob.z - z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace basalt::server::runtime::entity::mobBrain
