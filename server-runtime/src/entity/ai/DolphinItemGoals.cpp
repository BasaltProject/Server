#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/DolphinItemGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity::ai {
namespace {

namespace mth = wire::common::mth;

/// parity: the `Dolphin.ALLOWED_ITEMS` box, eight blocks every way.
constexpr double kItemReach = 8.0;
constexpr double kItemSpeed = static_cast<double>(1.2F);
/// parity: `DolphinSwimToTreasureGoal`.
constexpr std::int32_t kTreasureAir = 100;
constexpr double kTreasureSpeed = 1.3;

[[nodiscard]] bool holding(const ServerEntity& mob) { return copperGolems::mainHand(mob).has_value(); }

} // namespace

bool DolphinMoveToItemGoal::canUse(MobContext& mob) {
    if (cooldown_ > mob.mob.tickCount) {
        return false;
    }
    return !mob.actions.itemsInWater(mob.mob, kItemReach).empty();
}

void DolphinMoveToItemGoal::start(MobContext& mob) {
    const std::vector<MobActions::ItemView> items = mob.actions.itemsInWater(mob.mob, kItemReach);
    if (!items.empty()) {
        static_cast<void>(
            navigation::moveTo(mob.world, mob.mob, items.front().x, items.front().y, items.front().z, kItemSpeed));
        mob.actions.mobSound(mob.mob, "entity.dolphin.play");
    }
    cooldown_ = 0;
}

void DolphinMoveToItemGoal::stop(MobContext& mob) {
    if (mob.actions.throwHeldItem(mob.mob)) {
        cooldown_ = mob.mob.tickCount + mob.random.nextInt(100);
    }
}

void DolphinMoveToItemGoal::tick(MobContext& mob) {
    const std::vector<MobActions::ItemView> items = mob.actions.itemsInWater(mob.mob, kItemReach);
    if (!items.empty()) {
        static_cast<void>(
            navigation::moveTo(mob.world, mob.mob, items.front().x, items.front().y, items.front().z, kItemSpeed));
    }
}

bool DolphinPlayWithItemsGoal::canUse(MobContext& mob) { return holding(mob.mob); }

void DolphinPlayWithItemsGoal::stop(MobContext& mob) { static_cast<void>(mob.actions.throwHeldItem(mob.mob)); }

void DolphinPlayWithItemsGoal::tick(MobContext& mob) { static_cast<void>(mob.actions.throwHeldItem(mob.mob)); }

bool DolphinSwimToTreasureGoal::canUse(MobContext& mob) { return mob.mob.gotFish && mob.mob.airSupply >= kTreasureAir; }

bool DolphinSwimToTreasureGoal::arrived(const MobContext& mob) const {
    if (!mob.mob.treasurePos.has_value()) {
        return false;
    }
    // parity: `BlockPos.containing(x, dolphin.y, z).closerToCenterThan(position, 4)`.
    const double dx = (*mob.mob.treasurePos)[0] + 0.5 - mob.mob.x;
    const double dy = std::floor(mob.mob.y) + 0.5 - mob.mob.y;
    const double dz = (*mob.mob.treasurePos)[2] + 0.5 - mob.mob.z;
    return (dx * dx) + (dy * dy) + (dz * dz) < 16.0;
}

bool DolphinSwimToTreasureGoal::canContinueToUse(MobContext& mob) {
    return mob.mob.treasurePos.has_value() && !arrived(mob) && !stuck_ && mob.mob.airSupply >= kTreasureAir;
}

void DolphinSwimToTreasureGoal::start(MobContext& mob) {
    stuck_ = false;
    navigation::stop(mob.mob);
    mob.mob.treasurePos = mob.actions.nearestTreasure(mob.mob);
    if (mob.mob.treasurePos.has_value()) {
        mob.actions.entityEvent(mob.mob, 38);
    } else {
        stuck_ = true;
    }
}

void DolphinSwimToTreasureGoal::stop(MobContext& mob) {
    if (!mob.mob.treasurePos.has_value() || arrived(mob) || stuck_) {
        mob.mob.gotFish = false;
        mob.actions.poseChanged(mob.mob);
    }
}

void DolphinSwimToTreasureGoal::tick(MobContext& mob) {
    if (!mob.mob.treasurePos.has_value()) {
        return;
    }
    // parity: `Dolphin.closeToNextPos`.
    bool close = false;
    if (mob.mob.walkTarget.has_value()) {
        const double dx = std::floor((*mob.mob.walkTarget)[0]) + 0.5 - mob.mob.x;
        const double dy = std::floor((*mob.mob.walkTarget)[1]) + 0.5 - mob.mob.y;
        const double dz = std::floor((*mob.mob.walkTarget)[2]) + 0.5 - mob.mob.z;
        close = (dx * dx) + (dy * dy) + (dz * dz) < 144.0;
    }
    if (!close && !navigation::isDone(mob.mob)) {
        return;
    }
    const randomPos::Spot target{(*mob.mob.treasurePos)[0] + 0.5, (*mob.mob.treasurePos)[1] + 0.5,
                                 (*mob.mob.treasurePos)[2] + 0.5};
    std::optional<randomPos::Spot> next = randomPos::defaultPosTowards(
        mob.world, mob.mob, mob.random, 16, 1, target, static_cast<double>(std::numbers::pi_v<float> / 8.0F));
    if (!next.has_value()) {
        next = randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 8, 4, target,
                                            static_cast<double>(std::numbers::pi_v<float> / 2.0F));
    }
    if (next.has_value()) {
        const std::optional<std::int32_t> state =
            mob.world.blockState(mth::floor((*next)[0]), mth::floor((*next)[1]), mth::floor((*next)[2]));
        if (!state.has_value() || !swimPath::holdsWater(*state)) {
            next = randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 8, 5, target,
                                                static_cast<double>(std::numbers::pi_v<float> / 2.0F));
        }
    }
    if (!next.has_value()) {
        stuck_ = true;
        return;
    }
    lookControl::lookAt(mob.mob, (*next)[0], (*next)[1], (*next)[2], 21.0F, 1.0F);
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, (*next)[0], (*next)[1], (*next)[2], kTreasureSpeed));
    if (mob.random.nextInt(adjustedTickDelay(80)) == 0) {
        mob.actions.entityEvent(mob.mob, 38);
    }
}

namespace {

/// parity: the boats and nautili within five blocks whose controlling passenger is a moving player.
[[nodiscard]] const PlayerView* riderNear(const MobContext& mob, bool mustMove) {
    for (const PlayerView& player : mob.players) {
        if (!player.vehicleId.has_value() || (mustMove && !player.movedHorizontally)) {
            continue;
        }
        const ServerEntity* vehicle = mob.entityById(*player.vehicleId);
        if (vehicle == nullptr || vehicle->passengerIds.empty() || vehicle->passengerIds.front() != player.entityId) {
            continue;
        }
        const std::string_view path = vehicle->type().path();
        if (!path.ends_with("_boat") && !path.ends_with("_raft") && path != "nautilus" && path != "zombie_nautilus") {
            continue;
        }
        const double reachX = (mob.mob.width() / 2.0) + 5.0 + (vehicle->width() / 2.0);
        if (std::abs(vehicle->x - mob.mob.x) < reachX && std::abs(vehicle->z - mob.mob.z) < reachX &&
            vehicle->y < mob.mob.y + mob.mob.height() + 5.0 && vehicle->y + vehicle->height() > mob.mob.y - 5.0) {
            return &player;
        }
    }
    return nullptr;
}

} // namespace

bool FollowRiddenBoatGoal::canUse(MobContext& mob) {
    if (following_.has_value()) {
        if (const PlayerView* player = mob.playerById(*following_); player != nullptr && player->movedHorizontally) {
            return true;
        }
    }
    return riderNear(mob, true) != nullptr;
}

bool FollowRiddenBoatGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* player = following_.has_value() ? mob.playerById(*following_) : nullptr;
    return player != nullptr && player->vehicleId.has_value() && player->movedHorizontally;
}

void FollowRiddenBoatGoal::start(MobContext& mob) {
    if (const PlayerView* player = riderNear(mob, false); player != nullptr) {
        following_ = player->entityId;
    }
    timeToRecalcPath_ = 0;
    ahead_ = false;
}

void FollowRiddenBoatGoal::stop(MobContext& /*mob*/) { following_.reset(); }

void FollowRiddenBoatGoal::tick(MobContext& mob) {
    const PlayerView* player = following_.has_value() ? mob.playerById(*following_) : nullptr;
    if (player == nullptr) {
        return;
    }
    // parity: an extra push along its own input, a hundredth ahead of the boat or a bit more behind.
    livingMovement::swimStep(mob.world, mob.mob, mob.mob.swimForward, mob.mob.swimUp,
                             static_cast<double>(ahead_ ? 0.01F : 0.015F));
    if (--timeToRecalcPath_ > 0) {
        return;
    }
    timeToRecalcPath_ = adjustedTickDelay(10);
    static constexpr std::array<std::array<std::int32_t, 2>, 4> kSteps{{{0, 1}, {-1, 0}, {0, -1}, {1, 0}}};
    const std::array<std::int32_t, 2>& step = kSteps[static_cast<std::size_t>(player->facing & 3)];
    const std::int32_t px = mth::floor(player->x);
    const std::int32_t py = mth::floor(player->y);
    const std::int32_t pz = mth::floor(player->z);
    const double distance = std::sqrt(mob.distanceSquaredTo(player->x, player->y, player->z));
    if (!ahead_) {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, px - step[0], py - 1, pz - step[1], 1.0));
        if (distance < 4.0) {
            timeToRecalcPath_ = 0;
            ahead_ = true;
        }
    } else {
        static_cast<void>(
            navigation::moveTo(mob.world, mob.mob, px + (step[0] * 10), py - 1, pz + (step[1] * 10), 1.0));
        if (distance > 12.0) {
            timeToRecalcPath_ = 0;
            ahead_ = false;
        }
    }
}

} // namespace basalt::server::runtime::entity::ai
