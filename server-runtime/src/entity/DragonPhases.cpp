#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::enderDragon {
namespace {

namespace mth = wire::common::mth;

constexpr float kRadiansPerDegree = static_cast<float>(std::numbers::pi / 180.0);
/// parity: the square distances outside which a phase looks for somewhere else to fly.
constexpr double kNearTarget = 100.0;
constexpr double kFarTarget = 22500.0;
constexpr std::int32_t kOuterRing = 12;

[[nodiscard]] Vec3 normalize(const Vec3& v) {
    const double length = std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
    if (length < static_cast<double>(1.0E-5F)) {
        return {};
    }
    return {v.x / length, v.y / length, v.z / length};
}

[[nodiscard]] double distanceSqr(const Vec3& a, double x, double y, double z) {
    return ((a.x - x) * (a.x - x)) + ((a.y - y) * (a.y - y)) + ((a.z - z) * (a.z - z));
}

/// parity: `EndPodiumFeature.getLocation`.
[[nodiscard]] std::array<std::int32_t, 3> egg(const DragonState& state, const DragonWorld& world, bool noLeaves) {
    const std::int32_t x = state.fightOrigin[0];
    const std::int32_t z = state.fightOrigin[2];
    return {x, noLeaves ? world.heightNoLeaves(x, z) : world.heightMotionBlocking(x, z), z};
}

/// parity: `Vec3i.distToCenterSqr`.
[[nodiscard]] double distToCenterSqr(const std::array<std::int32_t, 3>& block, double x, double y, double z) {
    const double dx = block[0] + 0.5 - x;
    const double dy = block[1] + 0.5 - y;
    const double dz = block[2] + 0.5 - z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

[[nodiscard]] Vec3 bottomCentre(const std::array<std::int32_t, 3>& block) {
    return {block[0] + 0.5, static_cast<double>(block[1]), block[2] + 0.5};
}

[[nodiscard]] bool crystalsLeft(const DragonWorld& world) {
    return world.aliveCrystals.has_value() && *world.aliveCrystals != 0;
}

/// parity: the three ways the phases fold a node index back onto a ring.
[[nodiscard]] std::int32_t onRing(std::int32_t index, bool outer) {
    if (outer) {
        index %= kOuterRing;
        if (index < 0) {
            index += kOuterRing;
        }
        return index;
    }
    return ((index - kOuterRing) & 7) + kOuterRing;
}

/// parity: `navigateToNextPathNode`, the next node lifted by up to twenty blocks.
void navigate(std::optional<Path>& path, std::optional<Vec3>& target, random::RandomSource& random) {
    if (!path.has_value() || path->isDone()) {
        return;
    }
    const std::array<std::int32_t, 3> current = path->nextNode();
    path->advance();
    float lifted = 0.0F;
    do {
        lifted = static_cast<float>(current[1]) + (random.nextFloat() * 20.0F);
    } while (lifted < static_cast<float>(current[1]));
    target = Vec3{static_cast<double>(current[0]), static_cast<double>(lifted), static_cast<double>(current[2])};
}

[[nodiscard]] std::optional<Path> circle(const ServerEntity& dragon, DragonState& state, DragonWorld& world,
                                         bool& clockwise, bool outer) {
    const bool left = crystalsLeft(world);
    const std::int32_t current = closestNode(state, dragon.x, dragon.y, dragon.z, left);
    std::int32_t target = current;
    if (world.random->nextInt(8) == 0) {
        clockwise = !clockwise;
        target += 6;
    }
    target += clockwise ? 1 : -1;
    std::optional<Path> path = findPath(state, current, onRing(target, outer), std::nullopt, left);
    if (path.has_value()) {
        path->advance();
    }
    return path;
}

void strafe(const ServerEntity& dragon, DragonState& state, DragonWorld& world, const Target& victim) {
    PhaseMemory& memory = state.memory;
    memory.strafeVictim = victim.entityId;
    const bool left = crystalsLeft(world);
    const std::int32_t current = closestNode(state, dragon.x, dragon.y, dragon.z, left);
    const std::int32_t target = closestNode(state, victim.x, victim.y, victim.z, left);
    const std::int32_t finalX = mth::floor(victim.x);
    const std::int32_t finalZ = mth::floor(victim.z);
    const double xd = finalX - dragon.x;
    const double zd = finalZ - dragon.z;
    const double flat = std::sqrt((xd * xd) + (zd * zd));
    const double lift = std::min(static_cast<double>(0.4F) + (flat / 80.0) - 1.0, 10.0);
    const std::int32_t finalY = mth::floor(victim.y + lift);
    memory.strafePath = findPath(state, current, target, std::array{finalX, finalY, finalZ}, left);
    if (memory.strafePath.has_value()) {
        memory.strafePath->advance();
        navigate(memory.strafePath, memory.strafeTarget, *world.random);
    }
}

void startStrafing(ServerEntity& dragon, DragonState& state, DragonWorld& world, const Target& victim) {
    setPhase(state, Phase::StrafePlayer, world);
    strafe(dragon, state, world, victim);
}

[[nodiscard]] bool needsNewTarget(const std::optional<Vec3>& target, const ServerEntity& dragon) {
    const double distance = target.has_value() ? distanceSqr(*target, dragon.x, dragon.y, dragon.z) : 0.0;
    return distance < kNearTarget || distance > kFarTarget;
}

void tickHolding(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    if (!needsNewTarget(memory.holdingTarget, dragon)) {
        return;
    }
    if (memory.holdingPath.has_value() && memory.holdingPath->isDone()) {
        const std::array<std::int32_t, 3> podium = egg(state, world, true);
        const std::int32_t crystals = world.aliveCrystals.value_or(0);
        if (world.random->nextInt(crystals + 3) == 0) {
            setPhase(state, Phase::LandingApproach, world);
            return;
        }
        const std::optional<Target> player =
            world.nearestPlayer(podium[0], podium[1], podium[2], TargetQuery{.needsSight = false});
        const double spread =
            player.has_value() ? distToCenterSqr(podium, player->x, player->y, player->z) / 512.0 : 64.0;
        if (player.has_value() && (world.random->nextInt(static_cast<std::int32_t>(spread + 2.0)) == 0 ||
                                   world.random->nextInt(crystals + 2) == 0)) {
            startStrafing(dragon, state, world, *player);
            return;
        }
    }
    if (!memory.holdingPath.has_value() || memory.holdingPath->isDone()) {
        // parity: `aliveCrystals() >= 0`, true whenever there is a fight at all.
        memory.holdingPath = circle(dragon, state, world, memory.holdingClockwise, world.aliveCrystals.has_value());
    }
    navigate(memory.holdingPath, memory.holdingTarget, *world.random);
}

void tickStrafe(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    const std::optional<Target> victim =
        memory.strafeVictim.has_value() ? world.playerById(*memory.strafeVictim) : std::nullopt;
    if (!victim.has_value()) {
        setPhase(state, Phase::HoldingPattern, world);
        return;
    }
    if (memory.strafePath.has_value() && memory.strafePath->isDone()) {
        const double xd = victim->x - dragon.x;
        const double zd = victim->z - dragon.z;
        const double flat = std::sqrt((xd * xd) + (zd * zd));
        const double lift = std::min(static_cast<double>(0.4F) + (flat / 80.0) - 1.0, 10.0);
        memory.strafeTarget = Vec3{victim->x, victim->y + lift, victim->z};
    }
    if (needsNewTarget(memory.strafeTarget, dragon)) {
        if (!memory.strafePath.has_value() || memory.strafePath->isDone()) {
            memory.strafePath = circle(dragon, state, world, memory.strafeClockwise, crystalsLeft(world));
        }
        navigate(memory.strafePath, memory.strafeTarget, *world.random);
    }
    const double apart = ((victim->x - dragon.x) * (victim->x - dragon.x)) +
                         ((victim->y - dragon.y) * (victim->y - dragon.y)) +
                         ((victim->z - dragon.z) * (victim->z - dragon.z));
    if (apart >= 4096.0 || !world.canSee(*victim)) {
        if (memory.fireballCharge > 0) {
            memory.fireballCharge--;
        }
        return;
    }
    memory.fireballCharge++;
    const Vec3 aim = normalize({victim->x - dragon.x, 0.0, victim->z - dragon.z});
    const float yaw = dragon.yaw * kRadiansPerDegree;
    const Vec3 facing = normalize({static_cast<double>(mth::sin(yaw)), 0.0, static_cast<double>(-mth::cos(yaw))});
    const auto dot = static_cast<float>((facing.x * aim.x) + (facing.y * aim.y) + (facing.z * aim.z));
    float angle = static_cast<float>(std::acos(static_cast<double>(dot)) * static_cast<double>(180.0F) /
                                     static_cast<double>(static_cast<float>(std::numbers::pi)));
    angle += 0.5F;
    if (memory.fireballCharge < 5 || angle < 0.0F || angle >= 10.0F) {
        return;
    }
    const Vec3 view = viewVector(dragon.pitch, dragon.yaw);
    const Vec3& head = state.parts[static_cast<std::size_t>(Part::Head)];
    const Vec3 from{head.x - view.x, head.y + 0.5 + 0.5, head.z - view.z};
    const Vec3 towards{victim->x - from.x, victim->y + (victim->height * 0.5) - from.y, victim->z - from.z};
    if (world.shootFireball) {
        world.shootFireball(from, normalize(towards));
    }
    memory.fireballCharge = 0;
    if (memory.strafePath.has_value()) {
        memory.strafePath->next = memory.strafePath->nodes.size();
    }
    setPhase(state, Phase::HoldingPattern, world);
}

void tickApproach(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    if (!needsNewTarget(memory.approachTarget, dragon)) {
        return;
    }
    if (!memory.approachPath.has_value() || memory.approachPath->isDone()) {
        const bool left = crystalsLeft(world);
        const std::int32_t current = closestNode(state, dragon.x, dragon.y, dragon.z, left);
        const std::array<std::int32_t, 3> podium = egg(state, world, true);
        const std::optional<Target> player =
            world.nearestPlayer(podium[0], podium[1], podium[2], TargetQuery{.needsSight = false});
        std::int32_t target = 0;
        if (player.has_value()) {
            const Vec3 aim = normalize({player->x, 0.0, player->z});
            target = closestNode(state, -aim.x * 40.0, 105.0, -aim.z * 40.0, left);
        } else {
            target = closestNode(state, 40.0, podium[1], 0.0, left);
        }
        memory.approachPath = findPath(state, current, target, podium, left);
        if (memory.approachPath.has_value()) {
            memory.approachPath->advance();
        }
    }
    navigate(memory.approachPath, memory.approachTarget, *world.random);
    if (memory.approachPath.has_value() && memory.approachPath->isDone()) {
        setPhase(state, Phase::Landing, world);
    }
}

void tickLanding(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    if (!memory.landingTarget.has_value()) {
        memory.landingTarget = bottomCentre(egg(state, world, true));
    }
    if (distanceSqr(*memory.landingTarget, dragon.x, dragon.y, dragon.z) < 1.0) {
        memory.flameCount = 0;
        setPhase(state, Phase::SittingScanning, world);
    }
}

void tickTakeoff(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    if (!memory.takeoffFirstTick && memory.takeoffPath.has_value()) {
        if (!(distToCenterSqr(egg(state, world, true), dragon.x, dragon.y, dragon.z) < 100.0)) {
            setPhase(state, Phase::HoldingPattern, world);
        }
        return;
    }
    memory.takeoffFirstTick = false;
    const bool left = crystalsLeft(world);
    const std::int32_t current = closestNode(state, dragon.x, dragon.y, dragon.z, left);
    const Vec3 look = headLookVector(dragon, state, world);
    const std::int32_t target = closestNode(state, -look.x * 40.0, 105.0, -look.z * 40.0, left);
    memory.takeoffPath = findPath(state, current, onRing(target, left), std::nullopt, left);
    if (!memory.takeoffPath.has_value()) {
        return;
    }
    // parity: the takeoff skips the first node before it looks for the next one.
    memory.takeoffPath->advance();
    navigate(memory.takeoffPath, memory.takeoffTarget, *world.random);
}

void tickFlaming(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    memory.flameTicks++;
    if (memory.flameTicks >= 200) {
        setPhase(state, memory.flameCount >= 4 ? Phase::Takeoff : Phase::SittingScanning, world);
        return;
    }
    if (memory.flameTicks != 10) {
        return;
    }
    const Vec3& head = state.parts[static_cast<std::size_t>(Part::Head)];
    const Vec3 look = normalize({head.x - dragon.x, 0.0, head.z - dragon.z});
    const double x = head.x + (look.x * 5.0 / 2.0);
    const double z = head.z + (look.z * 5.0 / 2.0);
    const double initialY = head.y + 0.5;
    double y = initialY;
    while (world.isEmptyBlock(mth::floor(x), mth::floor(y), mth::floor(z))) {
        if (--y < 0.0) {
            y = initialY;
            break;
        }
    }
    y = mth::floor(y) + 1;
    if (world.breathe) {
        memory.flameCloud = world.breathe(Vec3{x, y, z});
    }
}

void tickScanning(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    memory.scanningTime++;
    const std::optional<Target> near =
        world.nearestPlayer(dragon.x, dragon.y, dragon.z, TargetQuery{.range = 20.0, .maxHeightGap = 10.0});
    if (near.has_value()) {
        if (memory.scanningTime > 25) {
            setPhase(state, Phase::SittingAttacking, world);
            return;
        }
        const Vec3 aim = normalize({near->x - dragon.x, 0.0, near->z - dragon.z});
        const float yaw = dragon.yaw * kRadiansPerDegree;
        const Vec3 facing = normalize({static_cast<double>(mth::sin(yaw)), 0.0, static_cast<double>(-mth::cos(yaw))});
        const auto dot = static_cast<float>((facing.x * aim.x) + (facing.y * aim.y) + (facing.z * aim.z));
        const float angle = static_cast<float>(std::acos(static_cast<double>(dot)) * static_cast<double>(180.0F) /
                                               static_cast<double>(static_cast<float>(std::numbers::pi))) +
                            0.5F;
        if (angle >= 0.0F && angle <= 10.0F) {
            return;
        }
        const Vec3& head = state.parts[static_cast<std::size_t>(Part::Head)];
        const double xd = near->x - head.x;
        const double zd = near->z - head.z;
        const double delta = std::clamp(wrapDegrees(180.0 -
                                                    (vanillaAtan2(xd, zd) * static_cast<double>(180.0F) /
                                                     static_cast<double>(static_cast<float>(std::numbers::pi))) -
                                                    static_cast<double>(dragon.yaw)),
                                        -100.0, 100.0);
        state.yRotA *= 0.8F;
        float distance = static_cast<float>(std::sqrt((xd * xd) + (zd * zd))) + 1.0F;
        const float rotSpeed = distance;
        distance = std::min(distance, 40.0F);
        state.yRotA += static_cast<float>(delta) * (0.7F / distance / rotSpeed);
        dragon.yaw += state.yRotA;
        return;
    }
    if (memory.scanningTime < 100) {
        return;
    }
    const std::optional<Target> far = world.nearestPlayer(dragon.x, dragon.y, dragon.z, TargetQuery{.range = 150.0});
    setPhase(state, Phase::Takeoff, world);
    if (far.has_value()) {
        setPhase(state, Phase::ChargingPlayer, world);
        memory.chargeTarget = Vec3{far->x, far->y, far->z};
    }
}

void tickCharging(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    if (!memory.chargeTarget.has_value()) {
        setPhase(state, Phase::HoldingPattern, world);
        return;
    }
    if (memory.timeSinceCharge > 0 && memory.timeSinceCharge++ >= 10) {
        setPhase(state, Phase::HoldingPattern, world);
        return;
    }
    if (needsNewTarget(memory.chargeTarget, dragon)) {
        memory.timeSinceCharge++;
    }
}

void tickDying(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    PhaseMemory& memory = state.memory;
    memory.dyingTime++;
    if (!memory.dyingTarget.has_value()) {
        memory.dyingTarget = bottomCentre(egg(state, world, false));
    }
    // parity: the dragon holds on at one health until it is over the podium, then lets go.
    dragon.health = needsNewTarget(memory.dyingTarget, dragon) ? 0.0 : 1.0;
}

void begin(DragonState& state) {
    PhaseMemory& memory = state.memory;
    switch (state.phase) {
    case Phase::HoldingPattern:
        memory.holdingPath.reset();
        memory.holdingTarget.reset();
        break;
    case Phase::StrafePlayer:
        memory.fireballCharge = 0;
        memory.strafeTarget.reset();
        memory.strafePath.reset();
        memory.strafeVictim.reset();
        break;
    case Phase::LandingApproach:
        memory.approachPath.reset();
        memory.approachTarget.reset();
        break;
    case Phase::Landing:
        memory.landingTarget.reset();
        break;
    case Phase::Takeoff:
        memory.takeoffFirstTick = true;
        memory.takeoffPath.reset();
        memory.takeoffTarget.reset();
        break;
    case Phase::SittingFlaming:
        memory.flameTicks = 0;
        memory.flameCount++;
        break;
    case Phase::SittingScanning:
        memory.scanningTime = 0;
        break;
    case Phase::SittingAttacking:
        memory.attackingTicks = 0;
        break;
    case Phase::ChargingPlayer:
        memory.chargeTarget.reset();
        memory.timeSinceCharge = 0;
        break;
    case Phase::Dying:
        memory.dyingTarget.reset();
        memory.dyingTime = 0;
        break;
    case Phase::Hover:
        memory.hoverTarget.reset();
        break;
    }
}

void end(DragonState& state, DragonWorld* world) {
    // parity: only the flaming phase cleans up after itself, taking its breath cloud away.
    if (state.phase == Phase::SittingFlaming && state.memory.flameCloud.has_value()) {
        if (world != nullptr && world->stopBreathing) {
            world->stopBreathing(*state.memory.flameCloud);
        } else {
            state.staleClouds.push_back(*state.memory.flameCloud);
        }
        state.memory.flameCloud.reset();
    }
}

} // namespace

void setPhase(DragonState& state, Phase phase, DragonWorld& world) {
    if (state.phase == phase) {
        return;
    }
    end(state, &world);
    state.phase = phase;
    begin(state);
}

void setPhase(DragonState& state, Phase phase) {
    if (state.phase == phase) {
        return;
    }
    end(state, nullptr);
    state.phase = phase;
    begin(state);
}

void tickPhase(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    switch (state.phase) {
    case Phase::HoldingPattern:
        tickHolding(dragon, state, world);
        break;
    case Phase::StrafePlayer:
        tickStrafe(dragon, state, world);
        break;
    case Phase::LandingApproach:
        tickApproach(dragon, state, world);
        break;
    case Phase::Landing:
        tickLanding(dragon, state, world);
        break;
    case Phase::Takeoff:
        tickTakeoff(dragon, state, world);
        break;
    case Phase::SittingFlaming:
        tickFlaming(dragon, state, world);
        break;
    case Phase::SittingScanning:
        tickScanning(dragon, state, world);
        break;
    case Phase::SittingAttacking:
        if (state.memory.attackingTicks++ >= 40) {
            setPhase(state, Phase::SittingFlaming, world);
        }
        break;
    case Phase::ChargingPlayer:
        tickCharging(dragon, state, world);
        break;
    case Phase::Dying:
        tickDying(dragon, state, world);
        break;
    case Phase::Hover:
        if (!state.memory.hoverTarget.has_value()) {
            state.memory.hoverTarget = Vec3{dragon.x, dragon.y, dragon.z};
        }
        break;
    }
}

void strafeAfterCrystal(ServerEntity& dragon, DragonState& state, DragonWorld& world, const Target& player) {
    // parity: only the holding pattern reacts; `canAttack` is the query's own targetability.
    if (state.phase == Phase::HoldingPattern) {
        startStrafing(dragon, state, world, player);
    }
}

std::optional<Vec3> flyTarget(const DragonState& state) {
    const PhaseMemory& memory = state.memory;
    switch (state.phase) {
    case Phase::HoldingPattern:
        return memory.holdingTarget;
    case Phase::StrafePlayer:
        return memory.strafeTarget;
    case Phase::LandingApproach:
        return memory.approachTarget;
    case Phase::Landing:
        return memory.landingTarget;
    case Phase::Takeoff:
        return memory.takeoffTarget;
    case Phase::ChargingPlayer:
        return memory.chargeTarget;
    case Phase::Dying:
        return memory.dyingTarget;
    case Phase::Hover:
        return memory.hoverTarget;
    case Phase::SittingFlaming:
    case Phase::SittingScanning:
    case Phase::SittingAttacking:
        return std::nullopt;
    }
    return std::nullopt;
}

float flySpeed(const DragonState& state) {
    switch (state.phase) {
    case Phase::Landing:
        return 1.5F;
    case Phase::ChargingPlayer:
    case Phase::Dying:
        return 3.0F;
    case Phase::Hover:
        return 1.0F;
    default:
        return 0.6F;
    }
}

float turnSpeed(const ServerEntity& dragon, const DragonState& state) {
    const float rotSpeed =
        static_cast<float>(std::sqrt((dragon.velocityX * dragon.velocityX) + (dragon.velocityZ * dragon.velocityZ))) +
        1.0F;
    const float distance = std::min(rotSpeed, 40.0F);
    // parity: `DragonLandingPhase.getTurnSpeed` drops the 0.7 and one of the divisions.
    if (state.phase == Phase::Landing) {
        return distance / rotSpeed;
    }
    return 0.7F / distance / rotSpeed;
}

Vec3 viewVector(float pitch, float yaw) {
    const float xRad = pitch * kRadiansPerDegree;
    const float yRad = -yaw * kRadiansPerDegree;
    const float yCos = mth::cos(yRad);
    const float ySin = mth::sin(yRad);
    const float xCos = mth::cos(xRad);
    const float xSin = mth::sin(xRad);
    return {static_cast<double>(ySin * xCos), static_cast<double>(-xSin), static_cast<double>(yCos * xCos)};
}

Vec3 headLookVector(const ServerEntity& dragon, const DragonState& state, const DragonWorld& world) {
    if (state.phase == Phase::Landing || state.phase == Phase::Takeoff) {
        const std::array<std::int32_t, 3> podium = egg(state, world, true);
        const float distance =
            std::max(static_cast<float>(std::sqrt(distToCenterSqr(podium, dragon.x, dragon.y, dragon.z))) / 4.0F, 1.0F);
        const float lift = 6.0F / distance;
        return viewVector(-lift * 1.5F * 5.0F, dragon.yaw);
    }
    if (isSitting(state.phase)) {
        return viewVector(-45.0F, dragon.yaw);
    }
    return viewVector(dragon.pitch, dragon.yaw);
}

} // namespace basalt::server::runtime::entity::enderDragon
