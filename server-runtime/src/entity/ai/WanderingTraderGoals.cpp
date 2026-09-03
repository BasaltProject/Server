#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/WanderingTraderGoals.hpp>

#include <cmath>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr double kDirectPathDistance = 10.0;

bool isTooFarAway(const MobContext& mob, const std::array<std::int32_t, 3>& target, double distance) {
    const double dx = mob.mob.x - (target[0] + 0.5);
    const double dy = mob.mob.y - (target[1] + 0.5);
    const double dz = mob.mob.z - (target[2] + 0.5);
    return (dx * dx) + (dy * dy) + (dz * dz) >= distance * distance;
}

} // namespace

WanderingTraderWanderGoal::WanderingTraderWanderGoal(double stopDistance, double speedModifier)
    : stopDistance_(stopDistance), speedModifier_(speedModifier) {}

bool WanderingTraderWanderGoal::canUse(MobContext& mob) {
    return mob.mob.wanderTarget.has_value() && isTooFarAway(mob, *mob.mob.wanderTarget, stopDistance_);
}

bool WanderingTraderWanderGoal::canContinueToUse(MobContext& mob) { return canUse(mob); }

void WanderingTraderWanderGoal::stop(MobContext& mob) {
    mob.mob.wanderTarget.reset();
    navigation::stop(mob.mob);
}

void WanderingTraderWanderGoal::tick(MobContext& mob) {
    if (!mob.mob.wanderTarget.has_value() || !navigation::isDone(mob.mob)) {
        return;
    }
    const std::array<std::int32_t, 3>& target = *mob.mob.wanderTarget;
    const double dx = target[0] - mob.mob.x;
    const double dy = target[1] - mob.mob.y;
    const double dz = target[2] - mob.mob.z;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    if (distance > kDirectPathDistance) {
        // parity: it aims ten blocks along the line rather than at a point it cannot path to yet.
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, mob.mob.x + (dx / distance * kDirectPathDistance),
                                             mob.mob.y + (dy / distance * kDirectPathDistance),
                                             mob.mob.z + (dz / distance * kDirectPathDistance), speedModifier_));
        return;
    }
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, static_cast<double>(target[0]),
                                         static_cast<double>(target[1]), static_cast<double>(target[2]),
                                         speedModifier_));
}

MoveTowardsTraderHomeGoal::MoveTowardsTraderHomeGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool MoveTowardsTraderHomeGoal::canUse(MobContext& mob) {
    constexpr std::int32_t kHomeStep = 16;
    constexpr std::int32_t kHomeHeight = 7;
    constexpr std::int32_t kCandidatePasses = 4;

    if (!mob.mob.homePosition.has_value() || mob.mob.homeRadius <= 0) {
        return false;
    }
    const std::array<std::int32_t, 3>& home = *mob.mob.homePosition;
    const double dx = mob.mob.x - (home[0] + 0.5);
    const double dy = mob.mob.y - home[1];
    const double dz = mob.mob.z - (home[2] + 0.5);
    const auto radius = static_cast<double>(mob.mob.homeRadius);
    if ((dx * dx) + (dy * dy) + (dz * dz) <= radius * radius) {
        return false;
    }
    for (std::int32_t pass = 0; pass < kCandidatePasses; pass++) {
        const std::optional<std::array<double, 3>> candidate =
            mobBrain::strollTarget(mob.world, mob.mob, mob.random, kHomeStep, kHomeHeight);
        if (!candidate.has_value()) {
            continue;
        }
        const double towardX = home[0] + 0.5 - mob.mob.x;
        const double towardZ = home[2] + 0.5 - mob.mob.z;
        if ((((*candidate)[0] - mob.mob.x) * towardX) + (((*candidate)[2] - mob.mob.z) * towardZ) > 0.0) {
            wanted_ = candidate;
            return true;
        }
    }
    return false;
}

bool MoveTowardsTraderHomeGoal::canContinueToUse(MobContext& mob) { return !navigation::isDone(mob.mob); }

void MoveTowardsTraderHomeGoal::start(MobContext& mob) {
    if (!wanted_.has_value()) {
        return;
    }
    static_cast<void>(
        navigation::moveTo(mob.world, mob.mob, (*wanted_)[0], (*wanted_)[1], (*wanted_)[2], speedModifier_));
}

void MoveTowardsTraderHomeGoal::stop(MobContext& mob) {
    wanted_.reset();
    navigation::stop(mob.mob);
}

} // namespace basalt::server::runtime::entity::ai
