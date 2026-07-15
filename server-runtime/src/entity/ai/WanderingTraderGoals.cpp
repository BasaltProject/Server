#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/WanderingTraderGoals.hpp>

#include <cmath>
#include <cstdint>

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

namespace {

/// parity: `ItemUtils` use duration of a potion and of a bucket of milk.
constexpr std::int32_t kDrinkTicks = 32;
/// parity: `Level.isBrightOutside`, the sky darkness under which it counts as day.
constexpr std::int32_t kBrightSkyDarken = 4;

[[nodiscard]] bool invisible(const MobContext& mob) {
    return mob.mob.effects.contains(wire::common::Identifier::vanilla("invisibility")) ||
           (mob.mob.sourceNbt.has_value() && mob.mob.sourceNbt->getBoolean("Invisible"));
}

} // namespace

bool TraderUseItemGoal::canUse(MobContext& mob) {
    const mobMisc::State* held = mobMisc::peek(mob.mob);
    if (held != nullptr && (potion_ ? held->traderNoPotion : held->traderNoMilk)) {
        return false;
    }
    if (!mob.hasSkylight) {
        return false;
    }
    const bool bright = mob.skyDarken < kBrightSkyDarken;
    return potion_ ? !bright && !invisible(mob) : bright && invisible(mob);
}

bool TraderUseItemGoal::canContinueToUse(MobContext& /*mob*/) { return remaining_ > 0; }

void TraderUseItemGoal::start(MobContext& mob) {
    remaining_ = kDrinkTicks;
    mob.actions.traderUsesItem(mob.mob, potion_, UsePhase::Start);
}

void TraderUseItemGoal::tick(MobContext& mob) {
    if (remaining_ > 0 && --remaining_ == 0) {
        mob.actions.traderUsesItem(mob.mob, potion_, UsePhase::Finish);
    }
}

void TraderUseItemGoal::stop(MobContext& mob) {
    remaining_ = 0;
    mob.actions.traderUsesItem(mob.mob, potion_, UsePhase::Stop);
}

} // namespace basalt::server::runtime::entity::ai
