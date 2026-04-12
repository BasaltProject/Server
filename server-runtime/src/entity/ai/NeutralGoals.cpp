#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/NeutralGoals.hpp>

namespace basalt::server::runtime::entity::ai {
namespace {

/// parity: `LivingEntity.baseTick` forgets who hurt it a hundred ticks on.
constexpr std::int32_t kHurtMemoryTicks = 100;
/// parity: `ResetUniversalAngerTargetGoal.ALERT_RANGE_Y`.
constexpr double kAlertRangeY = 10.0;

} // namespace

bool angryAtPlayer(MobContext& mob, const PlayerView& player) {
    if (!player.targetable) {
        return false;
    }
    if (neutralAnger::isAngryAtAllPlayers(mob.mob, mob.gameTime, mob.universalAnger)) {
        return true;
    }
    const neutralAnger::Access access = neutralAnger::accessOf(mob.mob);
    return access.target != nullptr && access.target->has_value() && (*access.target)->toString() == player.uuid;
}

const PlayerView* nearestAngeringPlayer(MobContext& mob) {
    const double range = mob.mob.attributeWithModifiers("minecraft:follow_range");
    const PlayerView* best = nullptr;
    double bestDistance = range * range;
    for (const PlayerView& player : mob.players) {
        if (!angryAtPlayer(mob, player)) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(player.x, player.y, player.z);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = &player;
        }
    }
    return best;
}

bool AngryAtPlayerGoal::canUse(MobContext& mob) {
    if (mob.random.nextInt(reducedTickDelay(10)) != 0) {
        return false;
    }
    const PlayerView* best = nearestAngeringPlayer(mob);
    if (best == nullptr) {
        return false;
    }
    found_ = best->entityId;
    return true;
}

bool AngryAtPlayerGoal::canContinueToUse(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    const double range = mob.mob.attributeWithModifiers("minecraft:follow_range");
    return target.has_value() && mob.distanceSquaredTo(target->x, target->y, target->z) <= range * range;
}

void AngryAtPlayerGoal::start(MobContext& mob) { mob.mob.targetId = found_; }

void AngryAtPlayerGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

bool ResetUniversalAngerTargetGoal::canUse(MobContext& mob) {
    const ServerEntity& self = mob.mob;
    return mob.universalAnger && self.lastHurtByMob.has_value() &&
           self.tickCount - self.lastHurtByMobTimestamp <= kHurtMemoryTicks &&
           mob.playerById(*self.lastHurtByMob) != nullptr && self.lastHurtByMobTimestamp > lastHurtByPlayerTimestamp_;
}

void ResetUniversalAngerTargetGoal::start(MobContext& mob) {
    lastHurtByPlayerTimestamp_ = mob.mob.lastHurtByMobTimestamp;
    neutralAnger::refreshUniversalAnger(mob.mob, mob.gameTime, mob.random);
    if (!alertOthers_) {
        return;
    }
    // parity: the same kind in the unit cube at its feet grown by the follow range, and ten up or down.
    const double within = mob.mob.attributeWithModifiers("minecraft:follow_range");
    const double minX = mob.mob.x - within;
    const double maxX = mob.mob.x + 1.0 + within;
    const double minY = mob.mob.y - kAlertRangeY;
    const double maxY = mob.mob.y + 1.0 + kAlertRangeY;
    const double minZ = mob.mob.z - within;
    const double maxZ = mob.mob.z + 1.0 + within;
    for (ServerEntity* other : mob.entities) {
        if (other == &mob.mob || other->removed || other->type() != mob.mob.type()) {
            continue;
        }
        const double half = other->width() / 2.0;
        if (other->x + half < minX || other->x - half > maxX || other->y + other->height() < minY || other->y > maxY ||
            other->z + half < minZ || other->z - half > maxZ) {
            continue;
        }
        neutralAnger::refreshUniversalAnger(*other, mob.gameTime, mob.random);
    }
}

} // namespace basalt::server::runtime::entity::ai
