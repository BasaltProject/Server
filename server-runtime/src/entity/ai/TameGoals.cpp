#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/TameGoals.hpp>

#include <utility>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr double kPlayerEye = 1.62;
constexpr float kLookSpeed = 10.0F;

} // namespace

const PlayerView* ownerOf(const MobContext& mob) {
    if (!mob.mob.owner.has_value()) {
        return nullptr;
    }
    for (const PlayerView& player : mob.players) {
        if (player.uuid == *mob.mob.owner) {
            return &player;
        }
    }
    return nullptr;
}

bool SitWhenOrderedToGoal::canUse(MobContext& mob) { return mob.mob.sitting; }

bool SitWhenOrderedToGoal::canContinueToUse(MobContext& mob) { return mob.mob.sitting; }

void SitWhenOrderedToGoal::start(MobContext& mob) { navigation::stop(mob.mob); }

FollowOwnerGoal::FollowOwnerGoal(double speedModifier, double startDistance, double stopDistance)
    : speedModifier_(speedModifier), startDistance_(startDistance), stopDistance_(stopDistance) {}

bool FollowOwnerGoal::canUse(MobContext& mob) {
    if (mob.mob.sitting) {
        return false;
    }
    const PlayerView* owner = ownerOf(mob);
    return owner != nullptr && mob.distanceSquaredTo(owner->x, owner->y, owner->z) >= startDistance_ * startDistance_;
}

bool FollowOwnerGoal::canContinueToUse(MobContext& mob) {
    if (mob.mob.sitting) {
        return false;
    }
    const PlayerView* owner = ownerOf(mob);
    return owner != nullptr && mob.distanceSquaredTo(owner->x, owner->y, owner->z) > stopDistance_ * stopDistance_;
}

void FollowOwnerGoal::start(MobContext& /*mob*/) { repathCooldown_ = 0; }

void FollowOwnerGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

void FollowOwnerGoal::tick(MobContext& mob) {
    const PlayerView* owner = ownerOf(mob);
    if (owner == nullptr) {
        return;
    }
    lookControl::lookAt(mob.mob, owner->x, owner->y + kPlayerEye, owner->z, kLookSpeed);
    if (--repathCooldown_ > 0) {
        return;
    }
    repathCooldown_ = adjustedTickDelay(10);
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, owner->x, owner->y, owner->z, speedModifier_));
}

BegGoal::BegGoal(double lookDistance, std::set<std::string, std::less<>> items)
    : lookDistance_(lookDistance), items_(std::move(items)) {}

bool BegGoal::canUse(MobContext& mob) {
    player_.reset();
    double bestDistance = 0.0;
    for (const PlayerView& player : mob.players) {
        if (!player.targetable || !player.heldItem.has_value() || !items_.contains(player.heldItem->path())) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(player.x, player.y, player.z);
        if (distance > lookDistance_ * lookDistance_) {
            continue;
        }
        if (player_.has_value() && distance >= bestDistance) {
            continue;
        }
        bestDistance = distance;
        player_ = player.entityId;
    }
    return player_.has_value();
}

bool BegGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* holder = player_.has_value() ? mob.playerById(*player_) : nullptr;
    if (holder == nullptr || mob.distanceSquaredTo(holder->x, holder->y, holder->z) > lookDistance_ * lookDistance_) {
        return false;
    }
    return begTime_ > 0 && holder->heldItem.has_value() && items_.contains(holder->heldItem->path());
}

void BegGoal::start(MobContext& mob) {
    // parity: `BegGoal.start`, forty to eighty ticks of it.
    begTime_ = 40 + mob.random.nextInt(40);
    mob.mob.begging = true;
}

void BegGoal::stop(MobContext& mob) {
    mob.mob.begging = false;
    player_.reset();
}

void BegGoal::tick(MobContext& mob) {
    const PlayerView* holder = player_.has_value() ? mob.playerById(*player_) : nullptr;
    if (holder == nullptr) {
        return;
    }
    lookControl::lookAt(mob.mob, holder->x, holder->y + kPlayerEye, holder->z, kLookSpeed);
    begTime_--;
}

bool OwnerHurtByTargetGoal::canUse(MobContext& mob) {
    return mob.mob.owner.has_value() && !mob.mob.sitting && mob.mob.ownerHurtBy.has_value();
}

bool OwnerHurtByTargetGoal::canContinueToUse(MobContext& mob) { return targetOf(mob).has_value(); }

void OwnerHurtByTargetGoal::start(MobContext& mob) {
    mob.mob.targetId = mob.mob.ownerHurtBy;
    mob.mob.ownerHurtBy.reset();
}

void OwnerHurtByTargetGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

bool OwnerHurtTargetGoal::canUse(MobContext& mob) {
    return mob.mob.owner.has_value() && !mob.mob.sitting && mob.mob.ownerHurt.has_value();
}

bool OwnerHurtTargetGoal::canContinueToUse(MobContext& mob) { return targetOf(mob).has_value(); }

void OwnerHurtTargetGoal::start(MobContext& mob) {
    mob.mob.targetId = mob.mob.ownerHurt;
    mob.mob.ownerHurt.reset();
}

void OwnerHurtTargetGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

} // namespace basalt::server::runtime::entity::ai
