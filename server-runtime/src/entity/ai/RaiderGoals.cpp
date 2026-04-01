#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/RaiderEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/RaiderGoals.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/runtime/world/poi/PoiManager.hpp>

#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr double kRaidSpeed = 1.0;
constexpr double kBannerSpeed = 1.15;
constexpr double kVillageSpeed = 1.05;
constexpr double kBannerReach = 1.414;
constexpr double kBannerVertical = 8.0;
constexpr std::int64_t kUnreachableTicks = 600;
constexpr std::int32_t kRecruitInterval = 20;
constexpr std::int32_t kHomeSearch = 48;
constexpr std::int32_t kDistanceToPoi = 1;
constexpr double kQuarterTurn = static_cast<double>(std::numbers::pi_v<float> / 2.0F);
constexpr double kTenthTurn = static_cast<double>(std::numbers::pi_v<float> / 10.0F);

[[nodiscard]] world::poi::BlockPos blockOf(const ServerEntity& mob) {
    return {static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
            static_cast<std::int32_t>(std::floor(mob.z))};
}

[[nodiscard]] bool inVillage(MobContext& mob) { return mob.world.poi().isVillage(blockOf(mob.mob)); }

/// parity: `Raider.hasActiveRaid` and a raid not yet over.
[[nodiscard]] bool raiding(const MobContext& mob) {
    return mob.raid.has_value() && mob.raid->active && !mob.raid->over;
}

/// parity: `BlockPos.closerToCenterThan`.
[[nodiscard]] bool closerToCenter(const std::array<std::int32_t, 3>& pos, const ServerEntity& mob, double distance) {
    const double dx = pos[0] + 0.5 - mob.x;
    const double dy = pos[1] + 0.5 - mob.y;
    const double dz = pos[2] + 0.5 - mob.z;
    return (dx * dx) + (dy * dy) + (dz * dz) < distance * distance;
}

/// parity: `ObtainRaidLeaderBannerGoal.cannotPickUpBanner`.
[[nodiscard]] bool cannotPickUpBanner(const MobContext& mob) {
    return !raiding(mob) || !raiderEquipment::canBeLeader(mob.mob.type().path()) ||
           raiderEquipment::wearsOminousBanner(mob.mob) || mob.raid->leaderAlive;
}

/// The banners in reach, as `Raider.ALLOWED_ITEMS` keeps them.
[[nodiscard]] std::vector<world::DroppedItem> bannersNear(const MobContext& mob) {
    const double reach = mob.mob.attributeWithModifiers("minecraft:follow_range");
    const double half = mob.mob.width() / 2.0;
    std::vector<world::DroppedItem> found;
    for (world::DroppedItem& item :
         mob.world.nearbyItems(mob.mob.x, mob.mob.y, mob.mob.z, reach + kBannerVertical + mob.mob.height())) {
        const bool inside = std::abs(item.x - mob.mob.x) < half + reach &&
                            std::abs(item.z - mob.mob.z) < half + reach && item.y > mob.mob.y - kBannerVertical &&
                            item.y < mob.mob.y + mob.mob.height() + kBannerVertical;
        if (inside && item.canBePickedUp() &&
            raiderEquipment::isOminousBanner(item.item, item.count, item.components)) {
            found.push_back(std::move(item));
        }
    }
    return found;
}

} // namespace

bool ObtainRaidLeaderBannerGoal::canUse(MobContext& mob) {
    if (cannotPickUpBanner(mob)) {
        return false;
    }
    std::map<std::int32_t, std::int64_t> kept;
    for (const world::DroppedItem& banner : bannersNear(mob)) {
        const auto until = unreachable_.find(banner.entityId);
        if (until != unreachable_.end() && mob.gameTime < until->second) {
            kept.emplace(banner.entityId, until->second);
            continue;
        }
        if (navigation::canReach(mob.world, mob.mob, banner.x, banner.y, banner.z)) {
            banner_ = banner.entityId;
            bannerAt_ = {banner.x, banner.y, banner.z};
            return true;
        }
        kept.emplace(banner.entityId, mob.gameTime + kUnreachableTicks);
    }
    unreachable_ = std::move(kept);
    return false;
}

bool ObtainRaidLeaderBannerGoal::canContinueToUse(MobContext& mob) {
    if (!banner_.has_value()) {
        return false;
    }
    const std::vector<world::DroppedItem> near = mob.world.nearbyItems(bannerAt_[0], bannerAt_[1], bannerAt_[2], 1.0);
    const bool present =
        std::ranges::any_of(near, [this](const world::DroppedItem& item) { return item.entityId == *banner_; });
    return present && !navigation::isDone(mob.mob) && !cannotPickUpBanner(mob);
}

void ObtainRaidLeaderBannerGoal::start(MobContext& mob) {
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, bannerAt_[0], bannerAt_[1], bannerAt_[2], kBannerSpeed));
}

void ObtainRaidLeaderBannerGoal::stop(MobContext& /*mob*/) { banner_.reset(); }

void ObtainRaidLeaderBannerGoal::tick(MobContext& mob) {
    if (banner_.has_value() &&
        mob.distanceSquaredTo(bannerAt_[0], bannerAt_[1], bannerAt_[2]) < kBannerReach * kBannerReach) {
        mob.actions.takeRaidBanner(mob.mob, *banner_);
    }
}

bool PathfindToRaidGoal::canUse(MobContext& mob) {
    return !targetOf(mob).has_value() && mob.mob.passengerIds.empty() && raiding(mob) && !inVillage(mob);
}

bool PathfindToRaidGoal::canContinueToUse(MobContext& mob) { return raiding(mob) && !inVillage(mob); }

void PathfindToRaidGoal::tick(MobContext& mob) {
    if (!mob.raid.has_value() || !mob.raid->active) {
        return;
    }
    if (mob.mob.tickCount > recruitmentTick_) {
        recruitmentTick_ = mob.mob.tickCount + kRecruitInterval;
        mob.actions.recruitRaiders(mob.mob);
    }
    if (navigation::isDone(mob.mob)) {
        const auto& center = mob.raid->center;
        const std::optional<randomPos::Spot> towards = randomPos::defaultPosTowards(
            mob.world, mob.mob, mob.random, 15, 4, {center[0] + 0.5, static_cast<double>(center[1]), center[2] + 0.5},
            kQuarterTurn);
        if (towards.has_value()) {
            static_cast<void>(
                navigation::moveTo(mob.world, mob.mob, (*towards)[0], (*towards)[1], (*towards)[2], kRaidSpeed));
        }
    }
}

bool RaiderMoveThroughVillageGoal::canUse(MobContext& mob) {
    if (visited_.size() > 2) {
        visited_.erase(visited_.begin());
    }
    if (!raiding(mob)) {
        return false;
    }
    // parity: `PoiManager.getRandom`, the homes in range shuffled and the first not yet visited.
    std::vector<world::poi::PoiRecord> homes;
    mob.world.poi().inRange(world::poi::maskOf(world::poi::PoiType::kHome), blockOf(mob.mob), kHomeSearch,
                            world::poi::Occupancy::kAny, homes);
    for (std::size_t size = homes.size(); size > 1; --size) {
        const auto swapTo = static_cast<std::size_t>(mob.random.nextInt(static_cast<std::int32_t>(size)));
        std::swap(homes[size - 1], homes[swapTo]);
    }
    const auto found = std::ranges::find_if(homes, [this](const world::poi::PoiRecord& home) {
        return !std::ranges::contains(visited_, std::array<std::int32_t, 3>{home.pos.x, home.pos.y, home.pos.z});
    });
    if (found == homes.end()) {
        return false;
    }
    poi_ = {found->pos.x, found->pos.y, found->pos.z};
    return !targetOf(mob).has_value();
}

bool RaiderMoveThroughVillageGoal::canContinueToUse(MobContext& mob) {
    return !navigation::isDone(mob.mob) && !targetOf(mob).has_value() &&
           !closerToCenter(poi_, mob.mob, mob.mob.width() + kDistanceToPoi) && !stuck_;
}

void RaiderMoveThroughVillageGoal::start(MobContext& mob) {
    mob.mob.idleTicks = 0;
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, poi_[0], poi_[1], poi_[2], kVillageSpeed));
    stuck_ = false;
}

void RaiderMoveThroughVillageGoal::stop(MobContext& mob) {
    if (closerToCenter(poi_, mob.mob, kDistanceToPoi)) {
        visited_.push_back(poi_);
    }
}

void RaiderMoveThroughVillageGoal::tick(MobContext& mob) {
    if (!navigation::isDone(mob.mob)) {
        return;
    }
    const randomPos::Spot bottom{poi_[0] + 0.5, static_cast<double>(poi_[1]), poi_[2] + 0.5};
    std::optional<randomPos::Spot> next =
        randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 16, 7, bottom, kTenthTurn);
    if (!next.has_value()) {
        next = randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 8, 7, bottom, kQuarterTurn);
    }
    if (!next.has_value()) {
        stuck_ = true;
        return;
    }
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, (*next)[0], (*next)[1], (*next)[2], kVillageSpeed));
}

bool RaiderCelebrationGoal::canUse(MobContext& mob) {
    return mob.mob.health > 0.0 && !targetOf(mob).has_value() && mob.raid.has_value() && mob.raid->loss;
}

void RaiderCelebrationGoal::start(MobContext& mob) { mob.actions.setCelebrating(mob.mob, true); }

void RaiderCelebrationGoal::stop(MobContext& mob) { mob.actions.setCelebrating(mob.mob, false); }

void RaiderCelebrationGoal::tick(MobContext& mob) {
    const std::string_view path = mob.mob.type().path();
    const bool silent = mob.mob.sourceNbt.has_value() && mob.mob.sourceNbt->getBoolean("Silent");
    if (!silent && mob.random.nextInt(adjustedTickDelay(100)) == 0) {
        mob.actions.mobSound(mob.mob, path == "illusioner" ? std::string{"entity.illusioner.ambient"}
                                                           : "entity." + std::string{path} + ".celebrate");
    }
    if (!mob.mob.vehicleId.has_value() && mob.random.nextInt(adjustedTickDelay(50)) == 0) {
        livingMovement::jump(mob.world, mob.mob);
    }
}

} // namespace basalt::server::runtime::entity::ai
