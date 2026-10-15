#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/WitherGoals.hpp>

#include <optional>

namespace basalt::server::runtime::entity::ai {
namespace {

/// parity: the wither's `FOLLOW_RANGE`.
constexpr double kFollowRange = 40.0;

[[nodiscard]] std::optional<std::int32_t> nearest(MobContext& mob) {
    std::optional<std::int32_t> best;
    double bestDistance = kFollowRange * kFollowRange;
    for (const PlayerView& player : mob.players) {
        const double distance = mob.distanceSquaredTo(player.x, player.y, player.z);
        if (player.targetable && distance <= bestDistance) {
            bestDistance = distance;
            best = player.entityId;
        }
    }
    for (const ServerEntity* candidate : mob.entities) {
        if (candidate == nullptr || candidate == &mob.mob || candidate->removed || candidate->dying ||
            candidate->health <= 0.0 || !server::registry::entityAttributes::isLiving(candidate->type()) ||
            witherBoss::isFriend(candidate->type().path())) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(candidate->x, candidate->y, candidate->z);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = candidate->entityId();
        }
    }
    return best;
}

} // namespace

bool WitherDoNothingGoal::canUse(MobContext& mob) {
    return mob.mob.wither != nullptr && mob.mob.wither->invulnerableTicks > 0;
}

bool WitherTargetGoal::canUse(MobContext& mob) { return !mob.mob.targetId.has_value() && nearest(mob).has_value(); }

bool WitherTargetGoal::canContinueToUse(MobContext& mob) {
    if (!mob.mob.targetId.has_value()) {
        return false;
    }
    if (const PlayerView* player = mob.playerById(*mob.mob.targetId); player != nullptr) {
        return player->targetable &&
               mob.distanceSquaredTo(player->x, player->y, player->z) <= kFollowRange * kFollowRange;
    }
    const ServerEntity* target = mob.entityById(*mob.mob.targetId);
    return target != nullptr && !target->removed && target->health > 0.0 &&
           mob.distanceSquaredTo(target->x, target->y, target->z) <= kFollowRange * kFollowRange;
}

void WitherTargetGoal::start(MobContext& mob) { mob.mob.targetId = nearest(mob); }

void WitherTargetGoal::stop(MobContext& mob) { mob.mob.targetId.reset(); }

} // namespace basalt::server::runtime::entity::ai
