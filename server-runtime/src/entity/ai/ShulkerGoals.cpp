#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/ShulkerGoals.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <span>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace mth = wire::common::mth;

/// parity: `Shulker.ShulkerAttackGoal`, twenty blocks.
constexpr double kShotRangeSquared = 400.0;
/// parity: the four blocks the search box keeps along the attach axis.
constexpr double kShallow = 4.0;

[[nodiscard]] double followRange(const MobContext& mob) { return mob.mob.attributeWithModifiers("follow_range"); }

} // namespace

bool ShulkerPeekGoal::canUse(MobContext& mob) {
    if (mob.mob.targetId.has_value() || mob.random.nextInt(reducedTickDelay(40)) != 0) {
        return false;
    }
    shulkers::ShulkerState& state = shulkers::stateOf(mob.mob);
    return shulkers::canStayAt(mob.world, mob.mob, std::span<ServerEntity* const>{mob.entities},
                               {mth::floor(mob.mob.x), mth::floor(mob.mob.y), mth::floor(mob.mob.z)}, state.attachFace);
}

bool ShulkerPeekGoal::canContinueToUse(MobContext& mob) { return !mob.mob.targetId.has_value() && peekTime_ > 0; }

void ShulkerPeekGoal::start(MobContext& mob) {
    peekTime_ = adjustedTickDelay(20 * (1 + mob.random.nextInt(3)));
    mob.actions.shulkerPeek(mob.mob, 30);
}

void ShulkerPeekGoal::stop(MobContext& mob) {
    if (!mob.mob.targetId.has_value()) {
        mob.actions.shulkerPeek(mob.mob, 0);
    }
}

void ShulkerPeekGoal::tick(MobContext& /*mob*/) { --peekTime_; }

bool ShulkerAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void ShulkerAttackGoal::start(MobContext& mob) {
    attackTime_ = 20;
    mob.actions.shulkerPeek(mob.mob, 100);
}

void ShulkerAttackGoal::stop(MobContext& mob) { mob.actions.shulkerPeek(mob.mob, 0); }

void ShulkerAttackGoal::tick(MobContext& mob) {
    --attackTime_;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y + (target->height * 0.85), target->z, 180.0F, 180.0F);
    if (mob.distanceSquaredTo(target->x, target->y, target->z) >= kShotRangeSquared) {
        mob.mob.targetId.reset();
        return;
    }
    if (attackTime_ <= 0) {
        attackTime_ = 20 + (mob.random.nextInt(10) * 20 / 2);
        mob.actions.shootShulkerBullet(mob.mob, *mob.mob.targetId);
    }
}

bool ShulkerTargetGoal::canUse(MobContext& mob) {
    if (mob.mob.targetId.has_value() || mob.random.nextInt(reducedTickDelay(10)) != 0) {
        return false;
    }
    const double range = followRange(mob);
    const std::int32_t axis = registry::axisOf(shulkers::stateOf(mob.mob).attachFace);
    const std::array<double, 3> reach{axis == 0 ? kShallow : range, axis == 1 ? kShallow : range,
                                      axis == 2 ? kShallow : range};
    const PlayerView* best = nullptr;
    double bestDistance = range * range;
    for (const PlayerView& player : mob.players) {
        if (!player.targetable || std::abs(player.x - mob.mob.x) > reach[0] + 0.5 + (player.width / 2.0) ||
            player.y > mob.mob.y + 1.0 + reach[1] || player.y + player.height < mob.mob.y - reach[1] ||
            std::abs(player.z - mob.mob.z) > reach[2] + 0.5 + (player.width / 2.0)) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(player.x, player.y, player.z);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = &player;
        }
    }
    if (best == nullptr) {
        return false;
    }
    mob.mob.targetId = best->entityId;
    return true;
}

bool ShulkerTargetGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* target = mob.mob.targetId.has_value() ? mob.playerById(*mob.mob.targetId) : nullptr;
    if (target == nullptr || !target->targetable) {
        return false;
    }
    const double range = followRange(mob);
    return mob.distanceSquaredTo(target->x, target->y, target->z) <= range * range;
}

void ShulkerTargetGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

} // namespace basalt::server::runtime::entity::ai
