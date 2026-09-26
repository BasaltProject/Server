#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/TargetGoals.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace reg = server::registry;

constexpr double kDefaultFollowRange = 16.0;
constexpr double kEyeShare = 0.85;

/// parity: `getLightLevelDependentMagicValue() >= 0.5`, which is light eight and up.
constexpr std::int32_t kDarkness = 8;

[[nodiscard]] double followRange(const MobContext& mob) {
    return reg::entityAttributes::baseValue(mob.mob.type(), reg::entityAttributes::kFollowRange)
        .value_or(kDefaultFollowRange);
}

} // namespace

NearestAttackablePlayerGoal::NearestAttackablePlayerGoal(std::int32_t randomInterval, bool needsDarkness)
    : randomInterval_(randomInterval), needsDarkness_(needsDarkness) {}

bool NearestAttackablePlayerGoal::canUse(MobContext& mob) {
    if (mob.mob.targetId.has_value()) {
        return false;
    }
    if (randomInterval_ > 0 && mob.random.nextInt(reducedTickDelay(randomInterval_)) != 0) {
        return false;
    }
    if (needsDarkness_ && mob.light >= kDarkness) {
        return false;
    }
    return mob.nearestPlayer(followRange(mob)) != nullptr;
}

bool NearestAttackablePlayerGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* target = mob.mob.targetId.has_value() ? mob.playerById(*mob.mob.targetId) : nullptr;
    if (target == nullptr || !target->targetable) {
        return false;
    }
    const double range = followRange(mob);
    return mob.distanceSquaredTo(target->x, target->y, target->z) <= range * range;
}

void NearestAttackablePlayerGoal::start(MobContext& mob) {
    const PlayerView* nearest = mob.nearestPlayer(followRange(mob));
    mob.mob.targetId = nearest == nullptr ? std::nullopt : std::optional{nearest->entityId};
}

void NearestAttackablePlayerGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

NearestAttackableEntityGoal::NearestAttackableEntityGoal(std::set<std::string, std::less<>> targets,
                                                         std::int32_t randomInterval)
    : targets_(std::move(targets)), randomInterval_(randomInterval) {}

namespace {

[[nodiscard]] ServerEntity* nearestOf(MobContext& mob, const std::set<std::string, std::less<>>& targets) {
    const double range = followRange(mob);
    ServerEntity* best = nullptr;
    double bestDistance = range * range;
    for (ServerEntity* candidate : mob.entities) {
        if (candidate == nullptr || candidate == &mob.mob || candidate->removed ||
            !targets.contains(candidate->type().path()) || candidate->health <= 0.0) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(candidate->x, candidate->y, candidate->z);
        if (distance > bestDistance) {
            continue;
        }
        bestDistance = distance;
        best = candidate;
    }
    return best;
}

} // namespace

bool NearestAttackableEntityGoal::canUse(MobContext& mob) {
    if (mob.mob.targetId.has_value()) {
        return false;
    }
    if (randomInterval_ > 0 && mob.random.nextInt(reducedTickDelay(randomInterval_)) != 0) {
        return false;
    }
    return nearestOf(mob, targets_) != nullptr;
}

bool NearestAttackableEntityGoal::canContinueToUse(MobContext& mob) {
    ServerEntity* target = mob.mob.targetId.has_value() ? mob.entityById(*mob.mob.targetId) : nullptr;
    if (target == nullptr || target->removed || target->health <= 0.0) {
        return false;
    }
    const double range = followRange(mob);
    return mob.distanceSquaredTo(target->x, target->y, target->z) <= range * range;
}

void NearestAttackableEntityGoal::start(MobContext& mob) {
    const ServerEntity* nearest = nearestOf(mob, targets_);
    mob.mob.targetId = nearest == nullptr ? std::nullopt : std::optional{nearest->entityId()};
}

void NearestAttackableEntityGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

HurtByTargetGoal::HurtByTargetGoal(Types ignoreDamageFrom, bool alertOthers, Types ignoreAlert)
    : ignoreDamageFrom_(std::move(ignoreDamageFrom)), alertOthers_(alertOthers), ignoreAlert_(std::move(ignoreAlert)) {}

bool HurtByTargetGoal::canUse(MobContext& mob) {
    if (!mob.mob.hurtBy.has_value() || mob.mob.targetId.has_value()) {
        return false;
    }
    // parity: under `universal_anger` a player's hit is left to `ResetUniversalAngerTargetGoal`.
    if (mob.universalAnger && mob.playerById(*mob.mob.hurtBy) != nullptr) {
        mob.mob.hurtBy.reset();
        return false;
    }
    // parity: a hit from one of the ignored types is let pass, and forgotten.
    if (const ServerEntity* by = mob.entityById(*mob.mob.hurtBy);
        by != nullptr && ignoreDamageFrom_.contains(by->type().path())) {
        mob.mob.hurtBy.reset();
        return false;
    }
    return true;
}

bool HurtByTargetGoal::canContinueToUse(MobContext& mob) {
    if (!mob.mob.targetId.has_value()) {
        return false;
    }
    if (const PlayerView* player = mob.playerById(*mob.mob.targetId); player != nullptr) {
        return player->targetable;
    }
    const ServerEntity* entity = mob.entityById(*mob.mob.targetId);
    return entity != nullptr && !entity->removed;
}

void HurtByTargetGoal::start(MobContext& mob) {
    mob.mob.targetId = mob.mob.hurtBy;
    mob.mob.hurtBy.reset();
    if (!alertOthers_ || !mob.mob.targetId.has_value()) {
        return;
    }
    // parity: `Mob.hasLineOfSight`, eye to eye within 128 blocks.
    if (alertInSight_) {
        std::optional<std::array<double, 3>> eyes;
        if (const PlayerView* player = mob.playerById(*mob.mob.targetId); player != nullptr) {
            eyes = std::array<double, 3>{player->x, player->y + 1.62, player->z};
        } else if (const ServerEntity* by = mob.entityById(*mob.mob.targetId); by != nullptr) {
            eyes = std::array<double, 3>{by->x, by->y + mobBreathing::eyeHeightOf(*by), by->z};
        }
        const double ownEyes = mob.mob.y + mobBreathing::eyeHeightOf(mob.mob);
        if (!eyes.has_value() || mob.distanceSquaredTo((*eyes)[0], (*eyes)[1], (*eyes)[2]) > 128.0 * 128.0 ||
            blockRay::cast(mob.world, mob.mob.x, ownEyes, mob.mob.z, (*eyes)[0], (*eyes)[1], (*eyes)[2]).has_value()) {
            return;
        }
    }
    // parity: `alertOthers`, the same kind within the follow range across and ten up or down that
    // has nothing to fight yet.
    const double within = followRange(mob);
    for (ServerEntity* other : mob.entities) {
        if (other == &mob.mob || other->removed || other->dying || other->type() != mob.mob.type() ||
            other->targetId.has_value() || ignoreAlert_.contains(other->type().path())) {
            continue;
        }
        if (other->x + (other->width() / 2.0) < mob.mob.x - within ||
            other->x - (other->width() / 2.0) > mob.mob.x + 1.0 + within ||
            other->y + other->height() < mob.mob.y - 10.0 || other->y > mob.mob.y + 11.0 ||
            other->z + (other->width() / 2.0) < mob.mob.z - within ||
            other->z - (other->width() / 2.0) > mob.mob.z + 1.0 + within) {
            continue;
        }
        if (mob.mob.owner.has_value() && other->owner != mob.mob.owner) {
            continue;
        }
        other->targetId = mob.mob.targetId;
    }
}

void HurtByTargetGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

MeleeAttackGoal::MeleeAttackGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool MeleeAttackGoal::withinReach(const MobContext& mob, const TargetView& target) {
    const double reach =
        reg::entityTypeShapes::widthOf(mob.mob.type()) / 2.0 + mobCombat::attackReach() + target.width / 2.0;
    if (std::abs(target.x - mob.mob.x) > reach || std::abs(target.z - mob.mob.z) > reach) {
        return false;
    }
    return target.y <= mob.mob.y + reg::entityTypeShapes::heightOf(mob.mob.type()) &&
           target.y + target.height >= mob.mob.y;
}

bool MeleeAttackGoal::canUse(MobContext& mob) {
    // parity: `MeleeAttackGoal.canUse` asks its navigation for a path first and gives up when there
    // is none and the target is out of reach: that is what leaves a mob free to wander instead of
    // standing still facing somebody it cannot get to.
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return false;
    }
    return withinReach(mob, *target) || navigation::canReach(mob.world, mob.mob, target->x, target->y, target->z);
}

bool MeleeAttackGoal::canContinueToUse(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return false;
    }
    return withinReach(mob, *target) || !navigation::isDone(mob.mob);
}

void MeleeAttackGoal::start(MobContext& /*mob*/) { repathCooldown_ = 0; }

void MeleeAttackGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

void MeleeAttackGoal::tick(MobContext& mob) {
    constexpr std::int32_t kRepathMin = 4;
    constexpr std::int32_t kRepathSpread = 7;
    constexpr double kFarSquared = 1024.0;
    constexpr double kNearSquared = 256.0;
    constexpr std::int32_t kFarPenalty = 10;
    constexpr std::int32_t kNearPenalty = 5;
    constexpr std::int32_t kNoPathPenalty = 15;
    constexpr float kLookSpeed = 30.0F;

    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y + target->height * kEyeShare, target->z, kLookSpeed);
    repathCooldown_ = std::max(repathCooldown_ - 1, 0);
    if (repathCooldown_ <= 0) {
        repathCooldown_ = kRepathMin + mob.random.nextInt(kRepathSpread);
        const double distance = mob.distanceSquaredTo(target->x, target->y, target->z);
        if (distance > kFarSquared) {
            repathCooldown_ += kFarPenalty;
        } else if (distance > kNearSquared) {
            repathCooldown_ += kNearPenalty;
        }
        if (!navigation::moveTo(mob.world, mob.mob, target->x, target->y, target->z, speedModifier_)) {
            repathCooldown_ += kNoPathPenalty;
        }
    }
    if (mob.mob.attackCooldown > 0 || !withinReach(mob, *target)) {
        return;
    }
    mob.mob.attackCooldown = mobCombat::kAttackInterval;
    mob.actions.swing(mob.mob);
    const float damage = entityCombat::attackDamageOf(mob.mob);
    if (target->playerId.has_value()) {
        mob.actions.hurtPlayer(mob.mob, *target->playerId, damage);
    } else if (target->entity != nullptr) {
        mob.actions.hurtMob(mob.mob, *target->entity, damage);
    }
}

namespace {

/// Whoever the warden is angriest at, if they are still here and still worth hitting.
[[nodiscard]] std::optional<std::int32_t> suspect(MobContext& mob) {
    const std::optional<std::int32_t> angriest = wardens::target(mob.mob);
    if (!angriest.has_value()) {
        return std::nullopt;
    }
    // It has to still be here and still be worth hitting: a creative or spectating player is
    // neither, and a suspect who logged out or died is nobody to walk towards.
    if (const PlayerView* player = mob.playerById(*angriest); player != nullptr) {
        return player->targetable ? angriest : std::nullopt;
    }
    return mob.entityById(*angriest) != nullptr ? angriest : std::nullopt;
}

} // namespace

bool WardenBusyGoal::canUse(MobContext& mob) {
    const wardenBrain::WardenState* state = mob.mob.warden.get();
    return state != nullptr && (wardenBrain::isDiggingOrEmerging(mob.mob) || state->roarTicks > 0);
}

void WardenBusyGoal::start(MobContext& mob) { navigation::stop(mob.mob); }

bool AngerTargetGoal::canUse(MobContext& mob) { return suspect(mob).has_value(); }

bool AngerTargetGoal::canContinueToUse(MobContext& mob) { return suspect(mob).has_value(); }

void AngerTargetGoal::start(MobContext& mob) { tick(mob); }

void AngerTargetGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

void AngerTargetGoal::tick(MobContext& mob) {
    const std::optional<std::int32_t> angriest = suspect(mob);
    // parity: `SetRoarTarget`: somebody new is roared at first, and fought only once the roar ends.
    if (angriest.has_value() && !mob.mob.targetId.has_value()) {
        if (!wardenBrain::isBusy(mob.mob)) {
            mob.actions.wardenRoar(mob.mob, *angriest);
        }
        return;
    }
    mob.mob.targetId = angriest;
}

} // namespace basalt::server::runtime::entity::ai
