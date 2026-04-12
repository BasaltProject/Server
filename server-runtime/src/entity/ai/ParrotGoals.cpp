#include <basalt/server/runtime/entity/Parrots.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/ParrotGoals.hpp>

namespace basalt::server::runtime::entity::ai {
namespace {

[[nodiscard]] const PlayerView* ownerOf(const MobContext& mob) {
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

} // namespace

bool LandOnOwnersShoulderGoal::canUse(MobContext& mob) {
    const PlayerView* owner = ownerOf(mob);
    if (owner == nullptr) {
        return false;
    }
    const bool canBeSatOn = !owner->spectator && !owner->flying && !owner->inWater;
    return !mob.mob.sitting && canBeSatOn && parrots::canSitOnShoulder(mob.mob);
}

void LandOnOwnersShoulderGoal::start(MobContext& /*mob*/) { sittingOnShoulder_ = false; }

void LandOnOwnersShoulderGoal::tick(MobContext& mob) {
    if (sittingOnShoulder_ || mob.mob.sitting || mob.mob.sittingPose || mob.mob.leashHolderId.has_value()) {
        return;
    }
    const PlayerView* owner = ownerOf(mob);
    if (owner == nullptr) {
        return;
    }
    const double half = mob.mob.width() / 2.0;
    const double ownerHalf = owner->width / 2.0;
    const bool touching = mob.mob.x - half < owner->x + ownerHalf && mob.mob.x + half > owner->x - ownerHalf &&
                          mob.mob.y < owner->y + owner->height && mob.mob.y + mob.mob.height() > owner->y &&
                          mob.mob.z - half < owner->z + ownerHalf && mob.mob.z + half > owner->z - ownerHalf;
    if (touching) {
        sittingOnShoulder_ = mob.actions.sitOnShoulder(mob.mob, owner->entityId);
    }
}

} // namespace basalt::server::runtime::entity::ai
