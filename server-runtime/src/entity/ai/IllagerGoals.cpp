#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/IllagerGoals.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

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
    /// parity: `EvokerSummonSpellGoal`.
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
            mob.actions.applyEffect(*target->playerId, "blindness", kBlindnessTicks, 0, mob.mob.entityId());
        }
        break;
    }
}

bool WololoSpellGoal::canUse(MobContext& mob) {
    constexpr double kRange = 16.0;
    constexpr double kHeight = 4.0;
    constexpr std::int8_t kBlue = 11;
    if (mob.mob.targetId.has_value() || mob.mob.spellType != 0 || mob.mob.tickCount < nextCast_ ||
        !mob.actions.mobGriefing(mob.mob)) {
        return false;
    }
    std::vector<const ServerEntity*> sheep;
    for (const ServerEntity* candidate : mob.entities) {
        if (candidate == nullptr || candidate->removed || candidate->health <= 0.0 ||
            candidate->type().path() != "sheep" || !candidate->sourceNbt.has_value() ||
            candidate->sourceNbt->getByte("Color") != kBlue) {
            continue;
        }
        if (std::abs(candidate->x - mob.mob.x) <= kRange && std::abs(candidate->z - mob.mob.z) <= kRange &&
            std::abs(candidate->y - mob.mob.y) <= kHeight &&
            mob.distanceSquaredTo(candidate->x, candidate->y, candidate->z) <= kRange * kRange) {
            sheep.push_back(candidate);
        }
    }
    if (sheep.empty()) {
        return false;
    }
    mobMisc::of(mob.mob).wololoTarget = sheep[static_cast<std::size_t>(mob.random.nextInt(static_cast<std::int32_t>(sheep.size())))]->entityId();
    return true;
}

bool WololoSpellGoal::canContinueToUse(MobContext& mob) {
    const mobMisc::State* misc = mobMisc::peek(mob.mob);
    return misc != nullptr && misc->wololoTarget.has_value() && warmup_ > 0;
}

void WololoSpellGoal::start(MobContext& mob) {
    constexpr std::int32_t kWarmup = 40;
    constexpr std::int32_t kInterval = 140;
    warmup_ = adjustedTickDelay(kWarmup);
    nextCast_ = mob.mob.tickCount + kInterval;
    mob.actions.setCasting(mob.mob, true);
    navigation::stop(mob.mob);
}

void WololoSpellGoal::stop(MobContext& mob) {
    mob.actions.setCasting(mob.mob, false);
    mobMisc::of(mob.mob).wololoTarget.reset();
}

void WololoSpellGoal::tick(MobContext& mob) {
    if (--warmup_ != 0) {
        return;
    }
    if (const mobMisc::State* misc = mobMisc::peek(mob.mob); misc != nullptr && misc->wololoTarget.has_value()) {
        static_cast<void>(mob.actions.wololo(mob.mob, *misc->wololoTarget));
    }
}

} // namespace basalt::server::runtime::entity::ai
