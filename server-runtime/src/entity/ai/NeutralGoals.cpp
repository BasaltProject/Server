#include <basalt/server/runtime/entity/NeutralAnger.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/NeutralGoals.hpp>

namespace basalt::server::runtime::entity::ai {

bool AngryAtPlayerGoal::canUse(MobContext& mob) {
    if (mob.random.nextInt(reducedTickDelay(10)) != 0) {
        return false;
    }
    const neutralAnger::Anger& anger = neutralAnger::stateOf(mob.mob);
    if (!anger.target.has_value()) {
        return false;
    }
    const double range = mob.mob.attributeWithModifiers("minecraft:follow_range");
    const PlayerView* best = nullptr;
    double bestDistance = range * range;
    for (const PlayerView& player : mob.players) {
        if (!player.targetable || player.uuid != anger.target->toString()) {
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

} // namespace basalt::server::runtime::entity::ai
