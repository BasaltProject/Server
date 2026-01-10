#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/LongDistancePatrolGoal.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <cmath>
#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr double kSpeed = 0.7;
constexpr double kLeaderSpeed = 0.595;
constexpr double kCompanionRange = 16.0;
constexpr double kArrivedSquared = 100.0;
constexpr double kSideShare = 0.4;
constexpr double kStepDistance = 10.0;
constexpr std::int64_t kFailedPathCooldown = 200;
constexpr std::int32_t kRandomRadius = 8;
constexpr std::int32_t kRandomDiameter = 16;
constexpr std::int32_t kTargetRadius = 500;
constexpr std::int32_t kTargetDiameter = 1000;

const std::set<std::string, std::less<>>& raiderTypes() {
    static const std::set<std::string, std::less<>> kTypes{"pillager",   "vindicator", "evoker",
                                                           "illusioner", "witch",      "ravager"};
    return kTypes;
}

std::optional<std::int32_t> surfaceY(const MobContext& mob, std::int32_t x, std::int32_t z) {
    const server::world::Chunk* chunk = mob.world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return std::nullopt;
    }
    return chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::MotionBlockingNoLeaves);
}

void findPatrolTarget(ServerEntity& entity, MobContext& mob) {
    entity.patrolTarget = std::array<std::int32_t, 3>{
        static_cast<std::int32_t>(std::floor(entity.x)) - kTargetRadius + mob.random.nextInt(kTargetDiameter),
        static_cast<std::int32_t>(std::floor(entity.y)),
        static_cast<std::int32_t>(std::floor(entity.z)) - kTargetRadius + mob.random.nextInt(kTargetDiameter)};
    entity.patrolling = true;
}

void setPatrolTarget(ServerEntity& entity, const std::array<std::int32_t, 3>& target) {
    entity.patrolTarget = target;
    entity.patrolling = true;
}

} // namespace

bool LongDistancePatrolGoal::canUse(MobContext& mob) {
    return mob.mob.patrolling && !targetOf(mob).has_value() && mob.mob.passengerIds.empty() &&
           mob.mob.patrolTarget.has_value() && mob.gameTime >= mob.mob.patrolCooldownUntil;
}

bool LongDistancePatrolGoal::canContinueToUse(MobContext& mob) { return canUse(mob); }

std::vector<ServerEntity*> LongDistancePatrolGoal::companions(MobContext& mob) {
    const Aabb search =
        Aabb::of(mob.mob.x, mob.mob.y, mob.mob.z, mob.mob.width(), mob.mob.height()).inflate(kCompanionRange);
    std::vector<ServerEntity*> found;
    for (ServerEntity* candidate : mob.entities) {
        if (candidate == &mob.mob || candidate->removed || candidate->raidCenter.has_value() ||
            !raiderTypes().contains(candidate->type().path())) {
            continue;
        }
        if (search.intersects(
                Aabb::of(candidate->x, candidate->y, candidate->z, candidate->width(), candidate->height()))) {
            found.push_back(candidate);
        }
    }
    return found;
}

std::optional<std::array<std::int32_t, 3>>
LongDistancePatrolGoal::intermediateTarget(MobContext& mob, const std::array<std::int32_t, 3>& target) {
    const double targetX = target[0] + 0.5;
    const auto targetY = static_cast<double>(target[1]);
    const double targetZ = target[2] + 0.5;
    const double distanceX = mob.mob.x - targetX;
    const double distanceY = mob.mob.y - targetY;
    const double distanceZ = mob.mob.z - targetZ;
    // parity: it aims to one side of the line, which is what makes a patrol drift rather than march.
    const double sideX = targetX + (distanceZ * kSideShare);
    const double sideY = targetY + (distanceY * kSideShare);
    const double sideZ = targetZ - (distanceX * kSideShare);
    const double stepX = sideX - mob.mob.x;
    const double stepY = sideY - mob.mob.y;
    const double stepZ = sideZ - mob.mob.z;
    const double length = std::sqrt((stepX * stepX) + (stepY * stepY) + (stepZ * stepZ));
    if (length == 0.0) {
        return std::nullopt;
    }
    const auto x = static_cast<std::int32_t>(std::floor(mob.mob.x + (stepX / length * kStepDistance)));
    const auto z = static_cast<std::int32_t>(std::floor(mob.mob.z + (stepZ / length * kStepDistance)));
    const std::optional<std::int32_t> y = surfaceY(mob, x, z);
    if (!y.has_value()) {
        return std::nullopt;
    }
    return std::array<std::int32_t, 3>{x, *y, z};
}

bool LongDistancePatrolGoal::moveRandomly(MobContext& mob) {
    const std::int32_t x =
        static_cast<std::int32_t>(std::floor(mob.mob.x)) - kRandomRadius + mob.random.nextInt(kRandomDiameter);
    const std::int32_t z =
        static_cast<std::int32_t>(std::floor(mob.mob.z)) - kRandomRadius + mob.random.nextInt(kRandomDiameter);
    const std::optional<std::int32_t> y = surfaceY(mob, x, z);
    if (!y.has_value()) {
        return false;
    }
    return navigation::moveTo(mob.world, mob.mob, static_cast<double>(x), static_cast<double>(*y),
                              static_cast<double>(z), kSpeed);
}

void LongDistancePatrolGoal::tick(MobContext& mob) {
    if (!navigation::isDone(mob.mob)) {
        return;
    }
    const std::vector<ServerEntity*> nearby = companions(mob);
    if (mob.mob.patrolling && nearby.empty()) {
        mob.mob.patrolling = false;
        return;
    }
    if (!mob.mob.patrolTarget.has_value()) {
        return;
    }
    const std::array<std::int32_t, 3> target = *mob.mob.patrolTarget;
    if (mob.mob.patrolLeader &&
        mob.distanceSquaredTo(target[0] + 0.5, static_cast<double>(target[1]), target[2] + 0.5) < kArrivedSquared) {
        findPatrolTarget(mob.mob, mob);
        return;
    }
    const std::optional<std::array<std::int32_t, 3>> pathTarget = intermediateTarget(mob, target);
    if (!pathTarget.has_value()) {
        return;
    }
    const double speed = mob.mob.patrolLeader ? kLeaderSpeed : kSpeed;
    if (!navigation::moveTo(mob.world, mob.mob, static_cast<double>((*pathTarget)[0]),
                            static_cast<double>((*pathTarget)[1]), static_cast<double>((*pathTarget)[2]), speed)) {
        static_cast<void>(moveRandomly(mob));
        mob.mob.patrolCooldownUntil = mob.gameTime + kFailedPathCooldown;
        return;
    }
    if (mob.mob.patrolLeader) {
        for (ServerEntity* companion : nearby) {
            setPatrolTarget(*companion, *pathTarget);
        }
    }
}

} // namespace basalt::server::runtime::entity::ai
