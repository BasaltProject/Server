#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/FlyingGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr float kLookSpeed = 10.0F;
constexpr float kAttackLookSpeed = 30.0F;

bool isFree(const world::WorldState& world, double x, double y, double z) {
    const std::optional<std::int32_t> state =
        world.blockState(static_cast<std::int32_t>(std::floor(x)), static_cast<std::int32_t>(std::floor(y)),
                         static_cast<std::int32_t>(std::floor(z)));
    return state.has_value() && registry::blockShapes::isEmptyShape(*state);
}

/// The hit itself, which every one of the flying attacks lands the same way.
void strike(MobContext& mob, const TargetView& target) {
    mob.actions.swing(mob.mob);
    const float damage = entityCombat::attackDamageOf(mob.mob);
    if (target.playerId.has_value()) {
        mob.actions.hurtPlayer(mob.mob, *target.playerId, damage);
    } else if (target.entity != nullptr) {
        mob.actions.hurtMob(mob.mob, *target.entity, damage);
    }
}

} // namespace

FlyingWanderGoal::FlyingWanderGoal(std::int32_t range, double step, bool gated)
    : range_(range), step_(step), gated_(gated) {}

bool FlyingWanderGoal::canUse(MobContext& mob) {
    constexpr std::int32_t kInterval = 30;
    return arrived_ || mob.random.nextInt(reducedTickDelay(kInterval)) == 0;
}

bool FlyingWanderGoal::canContinueToUse(MobContext& /*mob*/) { return !arrived_; }

void FlyingWanderGoal::start(MobContext& mob) {
    const auto range = static_cast<double>(range_);
    wantedX_ = mob.mob.x + (((mob.random.nextDouble() * 2.0) - 1.0) * range);
    wantedY_ = mob.mob.y + (((mob.random.nextDouble() * 2.0) - 1.0) * static_cast<double>(range_ / 2));
    wantedZ_ = mob.mob.z + (((mob.random.nextDouble() * 2.0) - 1.0) * range);
    arrived_ = false;
}

void FlyingWanderGoal::tick(MobContext& mob) {
    // parity: `GhastMoveControl`, a tenth of a block every two to six ticks.
    constexpr std::int32_t kGateMin = 2;
    constexpr std::int32_t kGateSpread = 5;
    if (!isFree(mob.world, wantedX_, wantedY_, wantedZ_)) {
        arrived_ = true;
        return;
    }
    if (gated_ && floatDuration_-- > 0) {
        return;
    }
    if (gated_) {
        floatDuration_ = kGateMin + mob.random.nextInt(kGateSpread);
    }
    if (!flyingMovement::approach(mob.mob, wantedX_, wantedY_, wantedZ_, step_)) {
        arrived_ = true;
    }
    lookControl::lookAt(mob.mob, wantedX_, wantedY_, wantedZ_, kLookSpeed);
}

bool FireballAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void FireballAttackGoal::stop(MobContext& mob) {
    chargeTime_ = 0;
    mob.actions.setCharging(mob.mob, false);
}

void FireballAttackGoal::tick(MobContext& mob) {
    /// parity: sixty-four blocks, twenty ticks of charge, forty of rest, and the tell at ten.
    constexpr double kRangeSquared = 4096.0;
    constexpr std::int32_t kChargeTicks = 20;
    constexpr std::int32_t kCooldownTicks = 40;
    constexpr std::int32_t kChargeTell = 10;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    if (mob.distanceSquaredTo(target->x, target->y, target->z) < kRangeSquared) {
        lookControl::lookAt(mob.mob, target->x, target->y, target->z, kAttackLookSpeed);
        chargeTime_++;
        if (chargeTime_ == kChargeTicks) {
            mob.actions.shootFireball(mob.mob, target->x, target->y + (target->height / 2.0), target->z, true);
            chargeTime_ = -kCooldownTicks;
        }
    } else if (chargeTime_ > 0) {
        chargeTime_--;
    }
    mob.actions.setCharging(mob.mob, chargeTime_ > kChargeTell);
}

bool BlazeAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void BlazeAttackGoal::stop(MobContext& mob) {
    attackStep_ = 0;
    mob.actions.setCharging(mob.mob, false);
}

void BlazeAttackGoal::tick(MobContext& mob) {
    constexpr double kMeleeSquared = 4.0;
    constexpr std::int32_t kMeleeInterval = 20;
    constexpr std::int32_t kFirstDelay = 60;
    constexpr std::int32_t kBurstDelay = 6;
    constexpr std::int32_t kRestDelay = 100;
    constexpr std::int32_t kBurst = 4;
    /// parity: `travelFlying`'s own 0.02 air input, times what the move control asked for.
    constexpr double kChaseSpeed = 0.02;
    constexpr double kHover = 2.0;

    attackTime_--;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    const double distance = mob.distanceSquaredTo(target->x, target->y, target->z);
    lookControl::lookAt(mob.mob, target->x, target->y, target->z, kAttackLookSpeed);
    if (distance < kMeleeSquared) {
        if (attackTime_ <= 0) {
            attackTime_ = kMeleeInterval;
            strike(mob, *target);
        }
        static_cast<void>(flyingMovement::approach(mob.mob, target->x, target->y, target->z, kChaseSpeed));
        return;
    }
    if (attackTime_ <= 0) {
        attackStep_++;
        if (attackStep_ == 1) {
            attackTime_ = kFirstDelay;
            mob.actions.setCharging(mob.mob, true);
        } else if (attackStep_ <= kBurst) {
            attackTime_ = kBurstDelay;
        } else {
            attackTime_ = kRestDelay;
            attackStep_ = 0;
            mob.actions.setCharging(mob.mob, false);
        }
        if (attackStep_ > 1) {
            mob.actions.shootFireball(mob.mob, target->x, target->y + (target->height / 2.0), target->z, false);
        }
    }
    // parity: it hovers a little above whatever it is shooting at rather than closing in.
    static_cast<void>(flyingMovement::approach(mob.mob, target->x, target->y + kHover, target->z, kChaseSpeed));
}

bool SwoopAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void SwoopAttackGoal::tick(MobContext& mob) {
    constexpr double kCircleHeight = 6.0;
    constexpr double kCircleMargin = 2.0;
    constexpr double kDiveFloor = 0.5;
    /// parity: `Phantom.PhantomMoveControl`, which is the quickest of them.
    constexpr double kSwoopSpeed = 0.05;
    constexpr double kHitRange = 4.0;
    constexpr std::int32_t kHitInterval = 20;

    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y, target->z, kAttackLookSpeed);
    const double above = mob.mob.y - target->y;
    if (!diving_ && above > kCircleHeight) {
        diving_ = true;
    }
    if (diving_ && above < kDiveFloor) {
        diving_ = false;
    }
    const double wantedY = diving_ ? target->y : target->y + kCircleHeight + kCircleMargin;
    static_cast<void>(flyingMovement::approach(mob.mob, target->x, wantedY, target->z, kSwoopSpeed));
    // parity: the swoop hurts whatever it passes through.
    if (mob.distanceSquaredTo(target->x, target->y, target->z) > kHitRange || mob.mob.attackCooldown > 0) {
        return;
    }
    mob.mob.attackCooldown = kHitInterval;
    strike(mob, *target);
}

bool ChargeAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void ChargeAttackGoal::tick(MobContext& mob) {
    /// parity: `Vex.VexMoveControl`, a twentieth of a block a tick towards you.
    constexpr double kChargeSpeed = 0.05;
    constexpr double kHitRange = 2.25;
    constexpr std::int32_t kHitInterval = 20;

    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y, target->z, kAttackLookSpeed);
    static_cast<void>(
        flyingMovement::approach(mob.mob, target->x, target->y + (target->height / 2.0), target->z, kChargeSpeed));
    if (mob.distanceSquaredTo(target->x, target->y, target->z) > kHitRange || mob.mob.attackCooldown > 0) {
        return;
    }
    mob.mob.attackCooldown = kHitInterval;
    strike(mob, *target);
}

namespace {

/// parity: `Guardian`'s own eighty ticks of beam and the two points it adds.
constexpr std::int32_t kChargeTicks = 80;
constexpr float kBeamBonus = 2.0F;
constexpr double kKeepRange = 12.0;
constexpr double kSwimSpeed = 0.03;

} // namespace

bool BeamAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void BeamAttackGoal::start(MobContext& mob) {
    attackTime_ = -kChargeTicks;
    mob.actions.setBeamTarget(mob.mob, mob.mob.targetId);
}

void BeamAttackGoal::stop(MobContext& mob) {
    mob.actions.setBeamTarget(mob.mob, std::nullopt);
    attackTime_ = 0;
}

void BeamAttackGoal::tick(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y, target->z, kAttackLookSpeed);
    attackTime_++;
    if (attackTime_ == 0) {
        mob.actions.setBeamTarget(
            mob.mob,
            target->playerId.has_value()
                ? target->playerId
                : (target->entity != nullptr ? std::optional<std::int32_t>{target->entity->entityId()} : std::nullopt));
    } else if (attackTime_ >= kChargeTicks) {
        const float damage = entityCombat::attackDamageOf(mob.mob) + kBeamBonus;
        if (target->playerId.has_value()) {
            mob.actions.hurtPlayer(mob.mob, *target->playerId, damage);
        } else if (target->entity != nullptr) {
            mob.actions.hurtMob(mob.mob, *target->entity, damage);
        }
        mob.mob.targetId.reset();
        attackTime_ = -kChargeTicks;
    }
    // parity: it keeps its distance while the beam charges.
    if (std::sqrt(mob.distanceSquaredTo(target->x, target->y, target->z)) > kKeepRange) {
        static_cast<void>(flyingMovement::approach(mob.mob, target->x, target->y, target->z, kSwimSpeed));
    }
}

} // namespace basalt::server::runtime::entity::ai
