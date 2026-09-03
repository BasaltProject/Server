#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/VillagerGoals.hpp>
#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>
#include <limits>

namespace basalt::server::runtime::entity::ai {

namespace villageBlocks {

const Names& workstations() {
    static const Names kNames{
        "composter",         "smithing_table",  "barrel",     "blast_furnace", "brewing_stand", "cauldron",
        "cartography_table", "fletching_table", "grindstone", "lectern",       "loom",          "smoker",
        "stonecutter"};
    return kNames;
}

const Names& bells() {
    static const Names kNames{"bell"};
    return kNames;
}

const Names& beds() {
    static const Names kNames{"white_bed", "orange_bed", "magenta_bed",    "light_blue_bed", "yellow_bed", "lime_bed",
                              "pink_bed",  "gray_bed",   "light_gray_bed", "cyan_bed",       "purple_bed", "blue_bed",
                              "brown_bed", "green_bed",  "red_bed",        "black_bed"};
    return kNames;
}

bool matches(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const Names& names) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const std::optional<registry::BlockState> state = registry::blocks::byStateId(*stateId);
    return state.has_value() && names.contains(state->block.path());
}

std::optional<std::array<std::int32_t, 3>> findNearest(const world::WorldState& world, double fromX, double fromY,
                                                       double fromZ, const Names& names, std::int32_t range,
                                                       std::int32_t height) {
    const auto originX = static_cast<std::int32_t>(std::floor(fromX));
    const auto originY = static_cast<std::int32_t>(std::floor(fromY));
    const auto originZ = static_cast<std::int32_t>(std::floor(fromZ));
    std::optional<std::array<std::int32_t, 3>> best;
    std::int32_t bestDistance = std::numeric_limits<std::int32_t>::max();
    for (std::int32_t dy = -height; dy <= height; dy++) {
        for (std::int32_t dx = -range; dx <= range; dx++) {
            for (std::int32_t dz = -range; dz <= range; dz++) {
                const std::int32_t distance = (dx * dx) + (dy * dy) + (dz * dz);
                if (distance >= bestDistance) {
                    continue;
                }
                const std::int32_t x = originX + dx;
                const std::int32_t y = originY + dy;
                const std::int32_t z = originZ + dz;
                if (!matches(world, x, y, z, names)) {
                    continue;
                }
                best = std::array<std::int32_t, 3>{x, y, z};
                bestDistance = distance;
            }
        }
    }
    return best;
}

} // namespace villageBlocks

namespace {

constexpr std::int32_t kSearchInterval = 100;
constexpr std::int32_t kRepathInterval = 20;
constexpr double kArrived = 2.0;
constexpr std::array<std::array<std::int32_t, 2>, 4> kSides{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

} // namespace

WalkToBlockGoal::WalkToBlockGoal(double speed, const villageBlocks::Names& names, std::int32_t range,
                                 std::int32_t height)
    : speed_(speed), names_(&names), range_(range), height_(height) {}

bool WalkToBlockGoal::canUse(MobContext& mob) {
    if (!wanted(mob)) {
        return false;
    }
    if (found_ && villageBlocks::matches(mob.world, targetX_, targetY_, targetZ_, *names_)) {
        return true;
    }
    found_ = false;
    if (searchCooldown_ > 0) {
        searchCooldown_--;
        return false;
    }
    searchCooldown_ = kSearchInterval;
    const std::optional<std::array<std::int32_t, 3>> position =
        villageBlocks::findNearest(mob.world, mob.mob.x, mob.mob.y, mob.mob.z, *names_, range_, height_);
    if (!position.has_value()) {
        return false;
    }
    targetX_ = (*position)[0];
    targetY_ = (*position)[1];
    targetZ_ = (*position)[2];
    found_ = true;
    return true;
}

bool WalkToBlockGoal::canContinueToUse(MobContext& mob) {
    return found_ && wanted(mob) && villageBlocks::matches(mob.world, targetX_, targetY_, targetZ_, *names_);
}

void WalkToBlockGoal::start(MobContext& mob) {
    arrived_ = false;
    // A bed or a workstation is not something a mob can stand in, so it walks to the nearest spot
    // beside it and counts that as being there.
    const std::optional<std::array<double, 2>> spot = standingSpot(mob);
    if (!spot.has_value()) {
        return;
    }
    walkX_ = (*spot)[0];
    walkZ_ = (*spot)[1];
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, walkX_, static_cast<double>(targetY_), walkZ_, speed_));
}

void WalkToBlockGoal::stop(MobContext& mob) {
    if (arrived_) {
        onArrived(mob, false);
    }
    arrived_ = false;
    navigation::stop(mob.mob);
}

void WalkToBlockGoal::tick(MobContext& mob) {
    const double middleX = static_cast<double>(targetX_) + 0.5;
    const double middleZ = static_cast<double>(targetZ_) + 0.5;
    lookControl::lookAt(mob.mob, middleX, static_cast<double>(targetY_) + 0.5, middleZ);
    const bool close = mob.distanceSquaredTo(middleX, static_cast<double>(targetY_), middleZ) <= kArrived * kArrived;
    if (close != arrived_) {
        arrived_ = close;
        onArrived(mob, close);
    }
    if (close) {
        navigation::stop(mob.mob);
        return;
    }
    if (!navigation::isDone(mob.mob)) {
        return;
    }
    if (mob.mob.repathCooldown > 0) {
        mob.mob.repathCooldown--;
        return;
    }
    mob.mob.repathCooldown = kRepathInterval;
    if (!navigation::moveTo(mob.world, mob.mob, walkX_, static_cast<double>(targetY_), walkZ_, speed_)) {
        found_ = false;
    }
}

std::optional<std::array<double, 2>> WalkToBlockGoal::standingSpot(MobContext& mob) const {
    if (isPassable(mob.world, targetX_, targetY_, targetZ_)) {
        return std::array<double, 2>{static_cast<double>(targetX_) + 0.5, static_cast<double>(targetZ_) + 0.5};
    }
    for (const std::array<std::int32_t, 2>& side : kSides) {
        const std::int32_t x = targetX_ + side[0];
        const std::int32_t z = targetZ_ + side[1];
        if (!isPassable(mob.world, x, targetY_, z)) {
            continue;
        }
        return std::array<double, 2>{static_cast<double>(x) + 0.5, static_cast<double>(z) + 0.5};
    }
    return std::nullopt;
}

WorkAtSiteGoal::WorkAtSiteGoal(double speed) : WalkToBlockGoal(speed, villageBlocks::workstations(), 16) {}

bool WorkAtSiteGoal::wanted(MobContext& mob) {
    return !mob.raidNearby && villagerSchedule::activityAt(mob.dayTime) == villagerSchedule::Activity::Work;
}

MeetAtBellGoal::MeetAtBellGoal(double speed) : WalkToBlockGoal(speed, villageBlocks::bells(), 32) {}

bool MeetAtBellGoal::wanted(MobContext& mob) {
    return !mob.raidNearby && villagerSchedule::activityAt(mob.dayTime) == villagerSchedule::Activity::Meet;
}

SleepInBedGoal::SleepInBedGoal(double speed) : WalkToBlockGoal(speed, villageBlocks::beds(), 16) {}

bool SleepInBedGoal::wanted(MobContext& mob) {
    return mob.raidNearby || villagerSchedule::activityAt(mob.dayTime) == villagerSchedule::Activity::Rest;
}

void SleepInBedGoal::onArrived(MobContext& mob, bool arrived) {
    // parity: a villager hiding from a raid stands in the house rather than lying down.
    mob.actions.setSleeping(mob.mob, arrived && !mob.raidNearby);
}

} // namespace basalt::server::runtime::entity::ai
