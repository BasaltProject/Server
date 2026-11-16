#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/BreakDoorGoal.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime::entity::ai {
namespace {

/// parity: `BreakDoorGoal.getDoorBreakTime`, the 240 ticks a zombie takes at least.
constexpr std::int32_t kDoorBreakTime = 240;
/// parity: `LevelEvent.SOUND_ZOMBIE_ATTACK_WOODEN_DOOR` and `SOUND_ZOMBIE_BREAK_WOODEN_DOOR`.
constexpr std::int32_t kAttackDoorEvent = 1019;
constexpr std::int32_t kBreakDoorEvent = 1021;
constexpr std::int32_t kDestroyBlockEvent = 2001;
constexpr std::int32_t kNoCracks = 10;

[[nodiscard]] bool mayBreakDoors(const ServerEntity& mob) {
    return mob.sourceNbt.has_value() && mob.sourceNbt->getBoolean("CanBreakDoors");
}

[[nodiscard]] bool woodenDoor(world::WorldState& world, const std::array<std::int32_t, 3>& pos) {
    const std::optional<std::int32_t> state = world.blockState(pos[0], pos[1], pos[2]);
    return state.has_value() && pathFinder::isMobDoor(*state);
}

} // namespace

bool BreakDoorGoal::isOpen(MobContext& mob) {
    if (!hasDoor_) {
        return false;
    }
    const std::optional<std::int32_t> state = mob.world.blockState(doorPos_[0], doorPos_[1], doorPos_[2]);
    if (!state.has_value() || !pathFinder::isMobDoor(*state)) {
        hasDoor_ = false;
        return false;
    }
    const auto properties = server::registry::blockStateDefinitions::propertiesOf(*state);
    const auto open = properties.find("open");
    return open != properties.end() && open->second == "true";
}

bool BreakDoorGoal::canUse(MobContext& mob) {
    const ServerEntity& self = mob.mob;
    // parity: `DoorInteractGoal.canUse`.
    if (!mayBreakDoors(self) || !self.horizontallyBlocked || !self.path.has_value() ||
        self.pathIndex >= static_cast<std::int32_t>(self.path->size())) {
        return false;
    }
    const std::int32_t count = static_cast<std::int32_t>(self.path->size());
    bool found = false;
    for (std::int32_t index = 0; index < std::min(self.pathIndex + 2, count) && !found; ++index) {
        const std::array<std::int32_t, 3>& node = (*self.path)[static_cast<std::size_t>(index)];
        doorPos_ = {node[0], node[1] + 1, node[2]};
        const double dx = static_cast<double>(node[0]) - self.x;
        const double dz = static_cast<double>(node[2]) - self.z;
        if (!((dx * dx) + (dz * dz) > 2.25)) {
            hasDoor_ = woodenDoor(mob.world, doorPos_);
            found = hasDoor_;
        }
    }
    if (!found) {
        doorPos_ = {static_cast<std::int32_t>(std::floor(self.x)), static_cast<std::int32_t>(std::floor(self.y)) + 1,
                    static_cast<std::int32_t>(std::floor(self.z))};
        hasDoor_ = woodenDoor(mob.world, doorPos_);
        if (!hasDoor_) {
            return false;
        }
    }
    return mob.actions.mobGriefing(self) && mob.actions.hardDifficulty(self) && !isOpen(mob);
}

bool BreakDoorGoal::canContinueToUse(MobContext& mob) {
    const double dx = static_cast<double>(doorPos_[0]) + 0.5 - mob.mob.x;
    const double dy = static_cast<double>(doorPos_[1]) + 0.5 - mob.mob.y;
    const double dz = static_cast<double>(doorPos_[2]) + 0.5 - mob.mob.z;
    return breakTime_ <= kDoorBreakTime && !isOpen(mob) && (dx * dx) + (dy * dy) + (dz * dz) < 4.0 &&
           mob.actions.hardDifficulty(mob.mob);
}

void BreakDoorGoal::start(MobContext& /*mob*/) { breakTime_ = 0; }

void BreakDoorGoal::stop(MobContext& mob) {
    mob.actions.blockProgress(mob.mob, doorPos_[0], doorPos_[1], doorPos_[2], kNoCracks);
}

void BreakDoorGoal::tick(MobContext& mob) {
    if (mob.random.nextInt(20) == 0) {
        mob.actions.levelEvent(mob.mob, kAttackDoorEvent, doorPos_[0], doorPos_[1], doorPos_[2], 0);
        mob.actions.swing(mob.mob);
    }
    ++breakTime_;
    const auto progress =
        static_cast<std::int32_t>(static_cast<float>(breakTime_) / static_cast<float>(kDoorBreakTime) * 10.0F);
    if (progress != lastBreakProgress_) {
        mob.actions.blockProgress(mob.mob, doorPos_[0], doorPos_[1], doorPos_[2], progress);
        lastBreakProgress_ = progress;
    }
    if (breakTime_ == kDoorBreakTime && mob.actions.hardDifficulty(mob.mob)) {
        if (!mob.actions.breakDoor(mob.mob, doorPos_[0], doorPos_[1], doorPos_[2])) {
            start(mob);
            return;
        }
        mob.actions.levelEvent(mob.mob, kBreakDoorEvent, doorPos_[0], doorPos_[1], doorPos_[2], 0);
        mob.actions.levelEvent(mob.mob, kDestroyBlockEvent, doorPos_[0], doorPos_[1], doorPos_[2], 0);
    }
}

} // namespace basalt::server::runtime::entity::ai
