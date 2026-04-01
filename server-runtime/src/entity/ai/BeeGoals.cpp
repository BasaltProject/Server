#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/BeeGoals.hpp>
#include <basalt/server/runtime/entity/ai/BeeNavigation.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/TargetGoals.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>
#include <string>

namespace basalt::server::runtime::entity::ai::beeGoals {
namespace {

namespace mth = wire::common::mth;
using bees::BeeState;
using bees::Pos;
namespace nav = beeNavigation;

constexpr std::int32_t kMaxTravellingTicks = 2400;

[[nodiscard]] BeeState& beeOf(MobContext& mob) { return bees::stateOf(mob.mob, mob.random); }

[[nodiscard]] bool angry(MobContext& mob) { return bees::isAngry(beeOf(mob), mob.gameTime); }

[[nodiscard]] bool tooFar(const MobContext& mob, const Pos& pos) {
    return !bees::closerThan(mob.mob, pos, bees::kTooFarDistance);
}

/// parity: `getBeehiveBlockEntity`: nothing when there is no hive, it is too far or it is gone.
[[nodiscard]] std::optional<MobActions::HiveView> hiveOf(MobContext& mob) {
    const BeeState& bee = beeOf(mob);
    if (!bee.hive.has_value() || tooFar(mob, *bee.hive)) {
        return std::nullopt;
    }
    const MobActions::HiveView view = mob.actions.hiveAt(mob.mob, *bee.hive);
    return view.hive ? std::optional{view} : std::nullopt;
}

/// parity: `Bee.wantsToEnterHive`.
[[nodiscard]] bool wantsToEnterHive(MobContext& mob) {
    const BeeState& bee = beeOf(mob);
    if (bee.stayOutOfHive > 0 || bee.pollinating || bee.hasStung || targetOf(mob).has_value()) {
        return false;
    }
    const bool wants = bee.hasNectar || bee.ticksWithoutNectar > bees::kTicksWithoutNectarBeforeHome ||
                       mob.actions.beesStayInHive(mob.mob);
    if (!wants) {
        return false;
    }
    const std::optional<MobActions::HiveView> hive = hiveOf(mob);
    return !(hive.has_value() && hive->fireNearby);
}

/// parity: `Bee.attractsBees`: the tag, never waterlogged, and only a sunflower's top half.
[[nodiscard]] bool attractsBees(MobContext& mob, std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || !mob.actions.blockTagged("minecraft:bee_attractive", *block).value_or(false)) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    if (const auto waterlogged = properties.find("waterlogged");
        waterlogged != properties.end() && waterlogged->second == "true") {
        return false;
    }
    if (block->path() == "sunflower") {
        const auto half = properties.find("half");
        return half != properties.end() && half->second == "upper";
    }
    return true;
}

[[nodiscard]] Pos blockOf(const ServerEntity& mob) { return {mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z)}; }

/// parity: `Bee.BaseBeeGoal`: nothing a calm bee does while it is angry.
class BaseBeeGoal : public Goal {
public:
    [[nodiscard]] bool canUse(MobContext& mob) final { return canBeeUse(mob) && !angry(mob); }

    [[nodiscard]] bool canContinueToUse(MobContext& mob) final { return canBeeContinueToUse(mob) && !angry(mob); }

protected:
    [[nodiscard]] virtual bool canBeeUse(MobContext& mob) = 0;
    [[nodiscard]] virtual bool canBeeContinueToUse(MobContext& mob) = 0;
};

/// parity: `BeeAttackGoal`, a melee attack only while angry and with its sting.
class BeeAttackGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return inner_.flags(); }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    [[nodiscard]] bool canUse(MobContext& mob) override {
        return inner_.canUse(mob) && angry(mob) && !beeOf(mob).hasStung;
    }

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override {
        return inner_.canContinueToUse(mob) && angry(mob) && !beeOf(mob).hasStung;
    }

    void start(MobContext& mob) override { inner_.start(mob); }

    void stop(MobContext& mob) override { inner_.stop(mob); }

    void tick(MobContext& mob) override { inner_.tick(mob); }

private:
    MeleeAttackGoal inner_{1.4};
};

/// parity: `BeeEnterHiveGoal`.
class EnterHiveGoal final : public BaseBeeGoal {
public:
    void start(MobContext& mob) override {
        const BeeState& bee = beeOf(mob);
        if (bee.hive.has_value() && hiveOf(mob).has_value()) {
            mob.actions.enterHive(mob.mob, *bee.hive);
        }
    }

protected:
    [[nodiscard]] bool canBeeUse(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        if (!bee.hive.has_value() || !wantsToEnterHive(mob)) {
            return false;
        }
        // parity: `closerToCenterThan(position, 2)`.
        const double dx = (*bee.hive)[0] + 0.5 - mob.mob.x;
        const double dy = (*bee.hive)[1] + 0.5 - mob.mob.y;
        const double dz = (*bee.hive)[2] + 0.5 - mob.mob.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) >= 4.0) {
            return false;
        }
        const std::optional<MobActions::HiveView> hive = hiveOf(mob);
        if (!hive.has_value()) {
            return false;
        }
        if (!hive->full) {
            return true;
        }
        bee.hive.reset();
        return false;
    }

    [[nodiscard]] bool canBeeContinueToUse(MobContext& /*mob*/) override { return false; }
};

/// parity: `ValidateHiveGoal` and `ValidateFlowerGoal`, a look every one to two seconds.
class ValidateGoal final : public BaseBeeGoal {
public:
    ValidateGoal(bool hive, random::RandomSource& random) : hive_(hive), cooldown_(20 + random.nextInt(21)) {}

    void start(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        if (hive_) {
            if (bee.hive.has_value() && mob.world.blockState((*bee.hive)[0], (*bee.hive)[1], (*bee.hive)[2]) &&
                !hiveOf(mob).has_value()) {
                bees::dropHive(bee);
            }
        } else if (bee.flower.has_value()) {
            const std::optional<std::int32_t> state =
                mob.world.blockState((*bee.flower)[0], (*bee.flower)[1], (*bee.flower)[2]);
            if (state.has_value() && !attractsBees(mob, *state)) {
                bees::dropFlower(bee, mob.random);
            }
        }
        lastValidate_ = mob.gameTime;
    }

protected:
    [[nodiscard]] bool canBeeUse(MobContext& mob) override { return mob.gameTime > lastValidate_ + cooldown_; }

    [[nodiscard]] bool canBeeContinueToUse(MobContext& /*mob*/) override { return false; }

private:
    bool hive_;
    std::int32_t cooldown_;
    std::int64_t lastValidate_ = -1;
};

/// parity: `BeePollinateGoal`.
class PollinateGoal final : public BaseBeeGoal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool requiresUpdateEveryTick() const override { return true; }

    void start(MobContext& mob) override {
        successfulTicks_ = 0;
        pollinatingTicks_ = 0;
        lastSound_ = 0;
        BeeState& bee = beeOf(mob);
        bee.pollinating = true;
        bee.ticksWithoutNectar = 0;
    }

    void stop(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        if (successfulTicks_ > kMinTicks) {
            bees::setHasNectar(bee, true);
        }
        bee.pollinating = false;
        navigation::stop(mob.mob);
        bee.flowerCooldown = bees::kFlowerCooldown;
    }

    void tick(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        if (!bee.flower.has_value()) {
            return;
        }
        if (++pollinatingTicks_ > kMaxTicks) {
            bees::dropFlower(bee, mob.random);
            bee.pollinating = false;
            bee.flowerCooldown = bees::kFlowerCooldown;
            return;
        }
        const std::array<double, 3> flower{(*bee.flower)[0] + 0.5, (*bee.flower)[1] + static_cast<double>(0.6F),
                                           (*bee.flower)[2] + 0.5};
        if (distanceTo(mob, flower) > 1.0) {
            hover_ = flower;
            wantHover(mob);
            return;
        }
        if (!hover_.has_value()) {
            hover_ = flower;
        }
        const bool arrived = distanceTo(mob, *hover_) <= kArrival;
        if (!arrived && pollinatingTicks_ > kMaxTicks) {
            bees::dropFlower(bee, mob.random);
            return;
        }
        bool setWanted = true;
        if (arrived) {
            if (mob.random.nextInt(kPositionChangeChance) == 0) {
                const float offsetX = offset(mob);
                const float offsetZ = offset(mob);
                hover_ = std::array<double, 3>{flower[0] + offsetX, flower[1], flower[2] + offsetZ};
                navigation::stop(mob.mob);
            } else {
                setWanted = false;
            }
            lookControl::lookAt(mob.mob, flower[0], flower[1], flower[2]);
        }
        if (setWanted) {
            wantHover(mob);
        }
        ++successfulTicks_;
        if (mob.random.nextFloat() < 0.05F && successfulTicks_ > lastSound_ + 60) {
            lastSound_ = successfulTicks_;
            mob.actions.mobSound(mob.mob, "entity.bee.pollinate");
        }
    }

protected:
    [[nodiscard]] bool canBeeUse(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        if (bee.flowerCooldown > 0 || bee.hasNectar || mob.actions.raining(mob.mob)) {
            return false;
        }
        const std::optional<Pos> found = nearbyFlower(mob);
        if (!found.has_value()) {
            bee.flowerCooldown =
                bees::kFlowerRetryMin + mob.random.nextInt(bees::kFlowerRetryMax - bees::kFlowerRetryMin + 1);
            return false;
        }
        bee.flower = found;
        // parity: `moveTo(flower + 0.5, 1.2)`, a path to the flower's own square.
        static_cast<void>(nav::follow(mob, nav::plan(mob, *found, 1), 1.2));
        return true;
    }

    [[nodiscard]] bool canBeeContinueToUse(MobContext& mob) override {
        const BeeState& bee = beeOf(mob);
        if (!bee.pollinating || !bee.flower.has_value() || mob.actions.raining(mob.mob)) {
            return false;
        }
        return successfulTicks_ > kMinTicks ? mob.random.nextFloat() < 0.2F : true;
    }

private:
    static constexpr std::int32_t kMinTicks = 400;
    static constexpr std::int32_t kMaxTicks = 600;
    static constexpr double kArrival = 0.1;
    static constexpr std::int32_t kPositionChangeChance = 25;
    static constexpr std::int32_t kSearchRadius = 5;
    static constexpr std::int64_t kUnreachableTicks = 600;

    [[nodiscard]] static double distanceTo(const MobContext& mob, const std::array<double, 3>& at) {
        return std::sqrt(mob.distanceSquaredTo(at[0], at[1], at[2]));
    }

    [[nodiscard]] static float offset(MobContext& mob) {
        return ((mob.random.nextFloat() * 2.0F) - 1.0F) * 0.33333334F;
    }

    void wantHover(MobContext& mob) {
        moveControl::setWanted(mob.mob, (*hover_)[0], (*hover_)[1], (*hover_)[2], static_cast<double>(0.35F));
    }

    /// parity: `findNearbyFlower`, which forgets every square it did not meet again this search.
    [[nodiscard]] std::optional<Pos> nearbyFlower(MobContext& mob) {
        std::map<Pos, std::int64_t> kept;
        const std::optional<Pos> found = nav::firstByManhattan(blockOf(mob.mob), kSearchRadius, [&](const Pos& at) {
            const std::optional<std::int32_t> state = mob.world.blockState(at[0], at[1], at[2]);
            if (!state.has_value() || !attractsBees(mob, *state)) {
                return false;
            }
            const auto cached = unreachable_.find(at);
            if (cached != unreachable_.end() && mob.gameTime < cached->second) {
                kept[at] = cached->second;
                return false;
            }
            if (nav::plan(mob, at, 1).reaches) {
                return true;
            }
            kept[at] = mob.gameTime + kUnreachableTicks;
            return false;
        });
        unreachable_ = std::move(kept);
        return found;
    }

    std::int32_t successfulTicks_ = 0;
    std::int32_t pollinatingTicks_ = 0;
    std::int32_t lastSound_ = 0;
    std::optional<std::array<double, 3>> hover_;
    std::map<Pos, std::int64_t> unreachable_;
};

/// parity: `BeeLocateHiveGoal`.
class LocateHiveGoal final : public BaseBeeGoal {
public:
    void start(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        bee.hiveCooldown = bees::kHiveCooldown;
        std::vector<Pos> homes = mob.actions.beeHomesNear(mob.mob, bees::kHiveSearchDistance);
        std::erase_if(homes, [&](const Pos& at) {
            const MobActions::HiveView view = mob.actions.hiveAt(mob.mob, at);
            return !view.hive || view.full;
        });
        const Pos own = blockOf(mob.mob);
        const auto distance = [&own](const Pos& at) {
            const auto dx = static_cast<double>(at[0] - own[0]);
            const auto dy = static_cast<double>(at[1] - own[1]);
            const auto dz = static_cast<double>(at[2] - own[2]);
            return (dx * dx) + (dy * dy) + (dz * dz);
        };
        std::ranges::stable_sort(homes, {}, distance);
        if (homes.empty()) {
            return;
        }
        for (const Pos& home : homes) {
            if (std::ranges::find(bee.blacklistedHives, home) == bee.blacklistedHives.end()) {
                bee.hive = home;
                return;
            }
        }
        bee.blacklistedHives.clear();
        bee.hive = homes.front();
    }

protected:
    [[nodiscard]] bool canBeeUse(MobContext& mob) override {
        const BeeState& bee = beeOf(mob);
        return bee.hiveCooldown == 0 && !bee.hive.has_value() && wantsToEnterHive(mob);
    }

    [[nodiscard]] bool canBeeContinueToUse(MobContext& /*mob*/) override { return false; }
};

/// parity: `BeeGoToHiveGoal`.
class GoToHiveGoal final : public BaseBeeGoal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    void start(MobContext& /*mob*/) override {
        travellingTicks_ = 0;
        ticksStuck_ = 0;
    }

    void stop(MobContext& mob) override {
        travellingTicks_ = 0;
        ticksStuck_ = 0;
        navigation::stop(mob.mob);
    }

    void tick(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        if (!bee.hive.has_value()) {
            return;
        }
        const Pos hive = *bee.hive;
        if (++travellingTicks_ > adjustedTickDelay(kMaxTravellingTicks)) {
            dropAndBlacklist(bee);
            return;
        }
        if (!navigation::isDone(mob.mob)) {
            return;
        }
        if (!bees::closerThan(mob.mob, hive, bees::kPathfindToHiveWhenCloserThan)) {
            if (tooFar(mob, hive)) {
                bees::dropHive(bee);
            } else {
                nav::hopTowards(mob, hive);
                route_.reset();
            }
            return;
        }
        // parity: `pathfindDirectlyTowards`, ten times the searching and a square or two short.
        const std::int32_t closeEnough = bees::closerThan(mob.mob, hive, 3) ? 1 : 2;
        const nav::Route route = nav::plan(mob, hive, closeEnough, 10.0F);
        if (!route.reaches) {
            dropAndBlacklist(bee);
            return;
        }
        if (lastPath_.has_value() && *lastPath_ == route.nodes) {
            if (++ticksStuck_ > kTicksBeforeHiveDrop) {
                bees::dropHive(bee);
                ticksStuck_ = 0;
            }
        } else {
            lastPath_ = route.nodes;
        }
        static_cast<void>(nav::follow(mob, route, 1.0));
        route_ = route;
    }

protected:
    [[nodiscard]] bool canBeeUse(MobContext& mob) override {
        const BeeState& bee = beeOf(mob);
        if (!bee.hive.has_value() || tooFar(mob, *bee.hive) || !wantsToEnterHive(mob) || reached(mob, *bee.hive)) {
            return false;
        }
        const std::optional<std::int32_t> state = mob.world.blockState((*bee.hive)[0], (*bee.hive)[1], (*bee.hive)[2]);
        const wire::common::Identifier* block =
            state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
        return block != nullptr && world::beehives::isHive(block->path());
    }

    [[nodiscard]] bool canBeeContinueToUse(MobContext& mob) override { return canBeeUse(mob); }

private:
    static constexpr std::int32_t kTicksBeforeHiveDrop = 60;
    static constexpr std::size_t kMaxBlacklisted = 3;

    /// parity: `hasReachedTarget`: within two, or at the end of a path that reaches it.
    [[nodiscard]] bool reached(const MobContext& mob, const Pos& hive) const {
        if (bees::closerThan(mob.mob, hive, bees::kHiveCloseEnough)) {
            return true;
        }
        return route_.has_value() && route_->target == hive && route_->reaches && navigation::isDone(mob.mob);
    }

    void dropAndBlacklist(BeeState& bee) {
        if (bee.hive.has_value()) {
            bee.blacklistedHives.push_back(*bee.hive);
            while (bee.blacklistedHives.size() > kMaxBlacklisted) {
                bee.blacklistedHives.erase(bee.blacklistedHives.begin());
            }
        }
        bees::dropHive(bee);
    }

    std::int32_t travellingTicks_ = 0;
    std::int32_t ticksStuck_ = 0;
    std::optional<std::vector<Pos>> lastPath_;
    std::optional<nav::Route> route_;
};

/// parity: `BeeGoToKnownFlowerGoal`.
class GoToKnownFlowerGoal final : public BaseBeeGoal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    void start(MobContext& /*mob*/) override { travellingTicks_ = 0; }

    void stop(MobContext& mob) override {
        travellingTicks_ = 0;
        navigation::stop(mob.mob);
    }

    void tick(MobContext& mob) override {
        BeeState& bee = beeOf(mob);
        if (!bee.flower.has_value()) {
            return;
        }
        if (++travellingTicks_ > adjustedTickDelay(kMaxTravellingTicks)) {
            bees::dropFlower(bee, mob.random);
        } else if (navigation::isDone(mob.mob)) {
            if (tooFar(mob, *bee.flower)) {
                bees::dropFlower(bee, mob.random);
            } else {
                nav::hopTowards(mob, *bee.flower);
            }
        }
    }

protected:
    [[nodiscard]] bool canBeeUse(MobContext& mob) override {
        const BeeState& bee = beeOf(mob);
        return bee.flower.has_value() && bee.ticksWithoutNectar > bees::kTicksBeforeKnownFlower &&
               !bees::closerThan(mob.mob, *bee.flower, 2);
    }

    [[nodiscard]] bool canBeeContinueToUse(MobContext& mob) override { return canBeeUse(mob); }

private:
    std::int32_t travellingTicks_ = 0;
};

/// parity: `BeeGrowCropGoal`.
class GrowCropGoal final : public BaseBeeGoal {
public:
    void tick(MobContext& mob) override {
        if (mob.random.nextInt(adjustedTickDelay(kGrowChance)) != 0) {
            return;
        }
        BeeState& bee = beeOf(mob);
        const Pos own = blockOf(mob.mob);
        for (std::int32_t below = 1; below <= 2; ++below) {
            if (mob.actions.growBeeCrop(mob.mob, {own[0], own[1] - below, own[2]})) {
                ++bee.cropsGrown;
            }
        }
    }

protected:
    [[nodiscard]] bool canBeeUse(MobContext& mob) override {
        const BeeState& bee = beeOf(mob);
        if (bee.cropsGrown >= bees::kMaxCropsGrowable || mob.random.nextFloat() < 0.3F) {
            return false;
        }
        return bee.hasNectar && hiveOf(mob).has_value();
    }

    [[nodiscard]] bool canBeeContinueToUse(MobContext& mob) override { return canBeeUse(mob); }

private:
    static constexpr std::int32_t kGrowChance = 30;
};

/// parity: `BeeWanderGoal`, a hop every tenth idle turn, drawn back towards a far hive.
class WanderGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Move}; }

    [[nodiscard]] bool canUse(MobContext& mob) override {
        return navigation::isDone(mob.mob) && mob.random.nextInt(10) == 0;
    }

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override { return !navigation::isDone(mob.mob); }

    void start(MobContext& mob) override {
        const BeeState& bee = beeOf(mob);
        // parity: `getWanderThreshold`, forty-eight less sixteen for a homeless bee, less twenty-four else.
        const std::int32_t threshold =
            bees::kTooFarDistance - (!bee.hive.has_value() && !bee.flower.has_value() ? 16 : 24);
        const std::optional<Pos> hive = hiveOf(mob).has_value() ? bee.hive : std::nullopt;
        const std::optional<std::array<double, 3>> spot = nav::wanderSpot(mob, hive, threshold);
        if (!spot.has_value()) {
            return;
        }
        const Pos target{mth::floor((*spot)[0]), mth::floor((*spot)[1]), mth::floor((*spot)[2])};
        static_cast<void>(nav::follow(mob, nav::plan(mob, target, 1), 1.0));
    }
};

/// parity: `BeeHurtByOtherGoal`, which lets go once the anger is over.
class HurtByOtherGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return inner_.flags(); }

    [[nodiscard]] bool canUse(MobContext& mob) override { return inner_.canUse(mob); }

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override { return angry(mob) && inner_.canContinueToUse(mob); }

    void start(MobContext& mob) override { inner_.start(mob); }

    void stop(MobContext& mob) override { inner_.stop(mob); }

private:
    HurtByTargetGoal inner_{{}, true};
};

/// parity: `BeeBecomeAngryTargetGoal`: the nearest player it is angry at, while it still can sting.
class BecomeAngryTargetGoal final : public Goal {
public:
    [[nodiscard]] std::set<GoalFlag> flags() const override { return {GoalFlag::Target}; }

    [[nodiscard]] bool canUse(MobContext& mob) override {
        if (!canTarget(mob) || mob.random.nextInt(reducedTickDelay(10)) != 0) {
            return false;
        }
        const BeeState& bee = beeOf(mob);
        const double range = mob.mob.attributeWithModifiers("minecraft:follow_range");
        const PlayerView* best = nullptr;
        double bestDistance = range * range;
        for (const PlayerView& player : mob.players) {
            if (!player.targetable || !bee.angryAt.has_value() || player.uuid != bee.angryAt->toString()) {
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

    [[nodiscard]] bool canContinueToUse(MobContext& mob) override {
        if (!canTarget(mob) || !mob.mob.targetId.has_value()) {
            found_.reset();
            return false;
        }
        const std::optional<TargetView> target = targetOf(mob);
        const double range = mob.mob.attributeWithModifiers("minecraft:follow_range");
        return target.has_value() && mob.distanceSquaredTo(target->x, target->y, target->z) <= range * range;
    }

    void start(MobContext& mob) override { mob.mob.targetId = found_; }

    void stop(MobContext& mob) override { mob.mob.targetId.reset(); }

private:
    [[nodiscard]] static bool canTarget(MobContext& mob) { return angry(mob) && !beeOf(mob).hasStung; }

    std::optional<std::int32_t> found_;
};

template <typename GoalType, typename... Args>
void add(GoalSelector& selector, std::int32_t priority, Args&&... args) {
    selector.add(priority, std::make_unique<GoalType>(std::forward<Args>(args)...));
}

} // namespace

void registerGoals(GoalSelector& selector) {
    // parity: `ValidateHiveGoal` and `ValidateFlowerGoal` roll their cooldowns from the bee's random
    // when it is built; there is no bee yet here, so a source of its own stands in.
    static std::atomic<std::int64_t> seeds{0};
    random::RandomSource random = random::RandomSource::legacy(++seeds);
    add<BeeAttackGoal>(selector, 0);
    add<EnterHiveGoal>(selector, 1);
    add<BreedGoal>(selector, 2, 1.0);
    add<TemptGoal>(selector, 3, 1.25, breeding::foodFor(wire::common::Identifier::vanilla("bee")));
    add<ValidateGoal>(selector, 3, true, random);
    add<ValidateGoal>(selector, 3, false, random);
    add<PollinateGoal>(selector, 4);
    add<FollowParentGoal>(selector, 5, 1.25);
    add<LocateHiveGoal>(selector, 5);
    add<GoToHiveGoal>(selector, 5);
    add<GoToKnownFlowerGoal>(selector, 6);
    add<GrowCropGoal>(selector, 7);
    add<WanderGoal>(selector, 8);
    add<FloatGoal>(selector, 9);
    add<HurtByOtherGoal>(selector, 1);
    add<BecomeAngryTargetGoal>(selector, 2);
}

} // namespace basalt::server::runtime::entity::ai::beeGoals
