#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/EndermanGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/NeutralGoals.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::ai {
namespace {

/// parity: `isLookingAtMe(player, 0.025, true, false, eyeY)`.
constexpr double kCone = 0.025;
/// parity: `Mob.hasLineOfSight` gives up past 128 blocks.
constexpr double kSightRange = 128.0;
/// parity: the stare that teleports it away (4 blocks) and the chase that brings it close (16).
constexpr double kTooCloseSquared = 16.0;
constexpr double kFarSquared = 256.0;
constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0F;

[[nodiscard]] double playerEyeHeight(const PlayerView& player) {
    if (player.swimming) {
        return 0.4;
    }
    return player.sneaking ? 1.27 : 1.62;
}

[[nodiscard]] double distanceSquared(const ServerEntity& mob, const PlayerView& player) {
    const double dx = player.x - mob.x;
    const double dy = player.y - mob.y;
    const double dz = player.z - mob.z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

/// parity: `TargetingConditions.forCombat`.
[[nodiscard]] bool combatTarget(MobContext& mob, const PlayerView& player, bool ignoreSight) {
    if (!player.targetable) {
        return false;
    }
    const double range = mob.mob.attributeWithModifiers("minecraft:follow_range");
    const double seen = std::max(range * (player.sneaking ? 0.8 : 1.0), 2.0);
    if (distanceSquared(mob.mob, player) > seen * seen) {
        return false;
    }
    if (ignoreSight) {
        return true;
    }
    const double ownEye = mob.mob.y + mobBreathing::eyeHeightOf(mob.mob);
    const double theirEye = player.y + playerEyeHeight(player);
    return !blockRay::cast(mob.world, mob.mob.x, ownEye, mob.mob.z, player.x, theirEye, player.z).has_value();
}

} // namespace

namespace endermanGaze {

bool staredAtBy(MobContext& mob, const PlayerView& player) {
    if (player.gazeDisguised) {
        return false;
    }
    // parity: `Entity.calculateViewVector`, in floats off the sine table.
    const float pitch = player.pitch * kDegreesToRadians;
    const float yaw = -player.yaw * kDegreesToRadians;
    const float cosYaw = wire::common::mth::cos(yaw);
    const float sinYaw = wire::common::mth::sin(yaw);
    const float cosPitch = wire::common::mth::cos(pitch);
    const float sinPitch = wire::common::mth::sin(pitch);
    double lookX = static_cast<double>(sinYaw * cosPitch);
    double lookY = static_cast<double>(-sinPitch);
    double lookZ = static_cast<double>(cosYaw * cosPitch);
    const double lookLength = std::sqrt((lookX * lookX) + (lookY * lookY) + (lookZ * lookZ));
    if (lookLength < 1.0e-5) {
        return false;
    }
    lookX /= lookLength;
    lookY /= lookLength;
    lookZ /= lookLength;
    const double gazeY = mob.mob.y + mobBreathing::eyeHeightOf(mob.mob);
    const double eyeY = player.y + playerEyeHeight(player);
    double dx = mob.mob.x - player.x;
    double dy = gazeY - eyeY;
    double dz = mob.mob.z - player.z;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    if (distance < 1.0e-5) {
        return false;
    }
    dx /= distance;
    dy /= distance;
    dz /= distance;
    const double dot = (lookX * dx) + (lookY * dy) + (lookZ * dz);
    if (dot <= 1.0 - (kCone / distance)) {
        return false;
    }
    // parity: `LivingEntity.hasLineOfSight` from the player's eyes to the enderman's.
    if (distance > kSightRange) {
        return false;
    }
    return !blockRay::cast(mob.world, player.x, eyeY, player.z, mob.mob.x, gazeY, mob.mob.z).has_value();
}

bool teleportTowards(MobContext& mob, double x, double eyeY, double z) {
    double dirX = mob.mob.x - x;
    double dirY = (mob.mob.y + (mob.mob.height() * 0.5)) - eyeY;
    double dirZ = mob.mob.z - z;
    const double length = std::sqrt((dirX * dirX) + (dirY * dirY) + (dirZ * dirZ));
    if (length < 1.0e-5) {
        dirX = 0.0;
        dirY = 0.0;
        dirZ = 0.0;
    } else {
        dirX /= length;
        dirY /= length;
        dirZ /= length;
    }
    const double toX = mob.mob.x + ((mob.random.nextDouble() - 0.5) * 8.0) - (dirX * 16.0);
    const double toY = mob.mob.y + static_cast<double>(mob.random.nextInt(16) - 8) - (dirY * 16.0);
    const double toZ = mob.mob.z + ((mob.random.nextDouble() - 0.5) * 8.0) - (dirZ * 16.0);
    return teleportEnderman(mob, toX, toY, toZ);
}

} // namespace endermanGaze

bool EndermanLookForPlayerGoal::angerInducing(MobContext& mob, const PlayerView& player) {
    return endermanGaze::staredAtBy(mob, player) || angryAtPlayer(mob, player);
}

bool EndermanLookForPlayerGoal::canUse(MobContext& mob) {
    // parity: `getNearestPlayer` with the start conditions, the nearest that qualifies.
    pending_.reset();
    double best = 0.0;
    for (const PlayerView& player : mob.players) {
        if (!combatTarget(mob, player, false) || !angerInducing(mob, player)) {
            continue;
        }
        const double distance = distanceSquared(mob.mob, player);
        if (!pending_.has_value() || distance < best) {
            best = distance;
            pending_ = player.entityId;
        }
    }
    return pending_.has_value();
}

void EndermanLookForPlayerGoal::start(MobContext& mob) {
    aggroTime_ = adjustedTickDelay(5);
    teleportTime_ = 0;
    neutralAnger::stateOf(mob.mob).staredAt = true;
}

void EndermanLookForPlayerGoal::stop(MobContext& mob) {
    pending_.reset();
    target_.reset();
    // parity: `TargetGoal.stop` is `setTarget(null)`, which clears the stare too.
    mob.mob.targetId.reset();
    neutralAnger::stateOf(mob.mob).staredAt = false;
}

bool EndermanLookForPlayerGoal::canContinueToUse(MobContext& mob) {
    if (pending_.has_value()) {
        const PlayerView* pending = mob.playerById(*pending_);
        if (pending == nullptr || !angerInducing(mob, *pending)) {
            return false;
        }
        lookControl::lookAt(mob.mob, pending->x, pending->y + playerEyeHeight(*pending), pending->z, 10.0F, 10.0F);
        return true;
    }
    if (!target_.has_value()) {
        return false;
    }
    const PlayerView* target = mob.playerById(*target_);
    if (target == nullptr) {
        return false;
    }
    // parity: the continue conditions ignore sight and range; `TargetGoal` would check the rest.
    return target->targetable;
}

void EndermanLookForPlayerGoal::tick(MobContext& mob) {
    if (!mob.mob.targetId.has_value()) {
        target_.reset();
    }
    if (pending_.has_value()) {
        if (--aggroTime_ <= 0) {
            target_ = pending_;
            pending_.reset();
            mob.mob.targetId = target_;
        }
        return;
    }
    const PlayerView* target = target_.has_value() ? mob.playerById(*target_) : nullptr;
    if (target == nullptr || mob.mob.vehicleId.has_value()) {
        return;
    }
    if (endermanGaze::staredAtBy(mob, *target)) {
        if (distanceSquared(mob.mob, *target) < kTooCloseSquared) {
            static_cast<void>(teleportEndermanRandomly(mob));
        }
        teleportTime_ = 0;
    } else if (distanceSquared(mob.mob, *target) > kFarSquared && teleportTime_++ >= adjustedTickDelay(30) &&
               endermanGaze::teleportTowards(mob, target->x, target->y + playerEyeHeight(*target), target->z)) {
        teleportTime_ = 0;
    }
}

bool EndermanFreezeWhenLookedAtGoal::canUse(MobContext& mob) {
    target_.reset();
    const PlayerView* target = mob.mob.targetId.has_value() ? mob.playerById(*mob.mob.targetId) : nullptr;
    if (target == nullptr || distanceSquared(mob.mob, *target) > kFarSquared ||
        !endermanGaze::staredAtBy(mob, *target)) {
        return false;
    }
    target_ = target->entityId;
    return true;
}

void EndermanFreezeWhenLookedAtGoal::start(MobContext& mob) { navigation::stop(mob.mob); }

void EndermanFreezeWhenLookedAtGoal::tick(MobContext& mob) {
    const PlayerView* target = target_.has_value() ? mob.playerById(*target_) : nullptr;
    if (target != nullptr) {
        lookControl::lookAt(mob.mob, target->x, target->y + playerEyeHeight(*target), target->z);
    }
}

} // namespace basalt::server::runtime::entity::ai
