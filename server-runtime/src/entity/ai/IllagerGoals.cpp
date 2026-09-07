#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/IllagerGoals.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr float kLookSpeed = 30.0F;
constexpr double kEyeShare = 0.85;

} // namespace

CrossbowAttackGoal::CrossbowAttackGoal(double speedModifier, double attackRadius)
    : speedModifier_(speedModifier), attackRadius_(attackRadius) {}

bool CrossbowAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

bool CrossbowAttackGoal::canContinueToUse(MobContext& mob) { return targetOf(mob).has_value(); }

void CrossbowAttackGoal::stop(MobContext& mob) {
    charge_ = 0;
    seeTime_ = 0;
    mob.actions.setCharging(mob.mob, false);
    navigation::stop(mob.mob);
}

void CrossbowAttackGoal::tick(MobContext& mob) {
    constexpr std::int32_t kChargeTicks = 25;
    constexpr std::int32_t kMinDelay = 20;
    constexpr std::int32_t kDelaySpread = 20;
    constexpr std::int32_t kSeeTime = 5;
    constexpr double kAimShare = 0.5;
    constexpr float kArrowPower = 1.0F;

    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    const double distanceSquared = mob.distanceSquaredTo(target->x, target->y, target->z);
    seeTime_++;
    lookControl::lookAt(mob.mob, target->x, target->y + (target->height * kEyeShare), target->z, kLookSpeed);
    const bool needsToMove = distanceSquared > attackRadius_ * attackRadius_ || seeTime_ < kSeeTime;
    if (needsToMove && attackDelay_ == 0) {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, target->x, target->y, target->z, speedModifier_));
    } else {
        navigation::stop(mob.mob);
    }
    if (attackDelay_ > 0) {
        attackDelay_--;
        return;
    }
    // parity: `CrossbowItem.getChargeDuration`, twenty-five ticks of winding.
    charge_++;
    mob.actions.setCharging(mob.mob, true);
    if (charge_ < kChargeTicks) {
        return;
    }
    charge_ = 0;
    mob.actions.setCharging(mob.mob, false);
    mob.actions.shootArrow(mob.mob, target->x, target->y + (target->height * kAimShare), target->z, kArrowPower);
    attackDelay_ = kMinDelay + mob.random.nextInt(kDelaySpread);
}

bool ThrowPotionGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

bool ThrowPotionGoal::canContinueToUse(MobContext& mob) { return targetOf(mob).has_value(); }

void ThrowPotionGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

void ThrowPotionGoal::tick(MobContext& mob) {
    constexpr double kThrowRange = 10.0;
    constexpr double kSlownessRange = 8.0;
    constexpr double kWeaknessRange = 3.0;
    constexpr float kWeaknessChance = 0.25F;
    constexpr std::int32_t kThrowInterval = 60;
    constexpr double kWalkSpeed = 1.0;

    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y + (target->height * kEyeShare), target->z, kLookSpeed);
    const double distance = std::sqrt(mob.distanceSquaredTo(target->x, target->y, target->z));
    if (distance > kThrowRange) {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, target->x, target->y, target->z, kWalkSpeed));
    } else {
        navigation::stop(mob.mob);
    }
    if (attackDelay_ > 0) {
        attackDelay_--;
        return;
    }
    if (distance > kThrowRange) {
        return;
    }
    // parity: slowness past eight blocks, poison against anybody healthy, weakness up close.
    std::string_view potion = "poison";
    if (distance >= kSlownessRange) {
        potion = "slowness";
    } else if (mob.random.nextFloat() < kWeaknessChance && distance <= kWeaknessRange) {
        potion = "weakness";
    }
    mob.actions.throwPotion(mob.mob, target->x, target->y, target->z, potion);
    attackDelay_ = kThrowInterval;
}

CastSpellGoal::CastSpellGoal(Spell spell, std::int32_t castingTime, std::int32_t cooldown)
    : spell_(spell), castingTime_(castingTime), cooldown_(cooldown) {}

bool CastSpellGoal::canUse(MobContext& mob) {
    if (!targetOf(mob).has_value()) {
        return false;
    }
    if (nextCast_ > 0) {
        nextCast_--;
        return false;
    }
    return true;
}

bool CastSpellGoal::canContinueToUse(MobContext& mob) { return targetOf(mob).has_value() && casting_ < castingTime_; }

void CastSpellGoal::start(MobContext& mob) {
    casting_ = 0;
    mob.actions.setCasting(mob.mob, true);
    navigation::stop(mob.mob);
}

void CastSpellGoal::stop(MobContext& mob) {
    mob.actions.setCasting(mob.mob, false);
    nextCast_ = cooldown_;
}

void CastSpellGoal::tick(MobContext& mob) {
    /// parity: `EvokerSummonSpellGoal`, three vexes a cast, and the blindness an illusioner gives.
    constexpr std::int32_t kVexCount = 3;
    constexpr std::int32_t kBlindnessTicks = 400;

    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y, target->z, kLookSpeed);
    casting_++;
    if (casting_ < castingTime_) {
        return;
    }
    switch (spell_) {
    case Spell::Fangs:
        mob.actions.castFangs(mob.mob, target->x, target->y, target->z);
        break;
    case Spell::SummonVexes:
        mob.actions.summonVexes(mob.mob, kVexCount);
        break;
    case Spell::Blindness:
        if (target->playerId.has_value()) {
            mob.actions.applyEffect(*target->playerId, "blindness", kBlindnessTicks, 0);
        }
        break;
    }
}

namespace {

constexpr double kArrived = 12.0;
constexpr std::int32_t kRepathInterval = 40;
constexpr std::int32_t kSpread = 6;

} // namespace

PathfindToRaidGoal::PathfindToRaidGoal(double speed) : speed_(speed) {}

bool PathfindToRaidGoal::canUse(MobContext& mob) {
    if (!mob.mob.raidCenter.has_value() || targetOf(mob).has_value()) {
        return false;
    }
    const std::array<std::int32_t, 3>& centre = *mob.mob.raidCenter;
    return mob.distanceSquaredTo(centre[0] + 0.5, static_cast<double>(centre[1]), centre[2] + 0.5) >
           kArrived * kArrived;
}

bool PathfindToRaidGoal::canContinueToUse(MobContext& mob) { return canUse(mob); }

void PathfindToRaidGoal::start(MobContext& mob) { moveToCentre(mob); }

void PathfindToRaidGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

void PathfindToRaidGoal::tick(MobContext& mob) {
    if (!navigation::isDone(mob.mob)) {
        return;
    }
    if (mob.mob.repathCooldown > 0) {
        mob.mob.repathCooldown--;
        return;
    }
    mob.mob.repathCooldown = kRepathInterval;
    moveToCentre(mob);
}

void PathfindToRaidGoal::moveToCentre(MobContext& mob) const {
    if (!mob.mob.raidCenter.has_value()) {
        return;
    }
    const std::array<std::int32_t, 3>& centre = *mob.mob.raidCenter;
    // parity: it heads for the village rather than for its exact middle, which is often a wall.
    const std::int32_t toX = centre[0] + mob.random.nextInt((2 * kSpread) + 1) - kSpread;
    const std::int32_t toZ = centre[2] + mob.random.nextInt((2 * kSpread) + 1) - kSpread;
    static_cast<void>(
        navigation::moveTo(mob.world, mob.mob, toX + 0.5, static_cast<double>(centre[1]), toZ + 0.5, speed_));
}

} // namespace basalt::server::runtime::entity::ai
