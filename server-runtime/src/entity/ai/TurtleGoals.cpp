#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/entity/ai/TurtleGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>
#include <numbers>
#include <string_view>

namespace basalt::server::runtime::entity::ai {
namespace {

using Block = std::array<std::int32_t, 3>;

constexpr double kFarFromHome = 64.0;
constexpr double kNearHome = 7.0;
constexpr double kHomeLayRange = 9.0;
constexpr double kWithinHome = 16.0;
constexpr std::int32_t kGiveUpTicks = 600;
constexpr std::int32_t kLayTicks = 200;
constexpr std::int32_t kLayLoveTicks = 600;
constexpr std::int32_t kTravelSpan = 512;
constexpr std::int32_t kTravelDepth = 4;
constexpr std::int32_t kChunkMargin = 34;
constexpr std::int32_t kWaterGiveUpTicks = 1200;

[[nodiscard]] mobMisc::State& state(MobContext& mob) { return mobMisc::of(mob.mob); }

[[nodiscard]] Block homeOf(MobContext& mob) {
    mobMisc::State& held = state(mob);
    if (!held.turtleHome.has_value()) {
        held.turtleHome =
            Block{static_cast<std::int32_t>(std::floor(mob.mob.x)), static_cast<std::int32_t>(std::floor(mob.mob.y)),
                  static_cast<std::int32_t>(std::floor(mob.mob.z))};
    }
    return *held.turtleHome;
}

/// parity: `Vec3i.closerToCenterThan`.
[[nodiscard]] bool closerToHome(MobContext& mob, double distance) {
    const Block home = homeOf(mob);
    return mob.distanceSquaredTo(home[0] + 0.5, home[1] + 0.5, home[2] + 0.5) < distance * distance;
}

[[nodiscard]] bool isBaby(const MobContext& mob) { return mob.mob.age < 0; }

[[nodiscard]] std::string_view pathAt(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> id = mob.world.blockState(x, y, z);
    const std::optional<registry::BlockState> found = id.has_value() ? registry::blocks::byStateId(*id) : std::nullopt;
    return found.has_value() ? found->block.path() : std::string_view{};
}

[[nodiscard]] bool isAir(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::string_view path = pathAt(mob, x, y, z);
    return path == "air" || path == "cave_air" || path == "void_air";
}

/// parity: `TurtleEggBlock.isSand`.
[[nodiscard]] bool isSand(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> id = mob.world.blockState(x, y, z);
    const std::optional<registry::BlockState> found = id.has_value() ? registry::blocks::byStateId(*id) : std::nullopt;
    return found.has_value() && mob.actions.blockTagged("minecraft:sand", found->block).value_or(false);
}

[[nodiscard]] double towardsAngle(double divisor) {
    return static_cast<double>(static_cast<float>(std::numbers::pi) / static_cast<float>(divisor));
}

void setLayingEgg(MobContext& mob, bool on) {
    mobMisc::State& held = state(mob);
    held.turtleLayCounter = on ? 1 : 0;
    held.turtleLaying = on;
}

[[nodiscard]] randomPos::Spot bottomCenter(const Block& at) {
    return {at[0] + 0.5, static_cast<double>(at[1]), at[2] + 0.5};
}

} // namespace

TurtleLayEggGoal::TurtleLayEggGoal(double speedModifier) : MoveToBlockGoal(speedModifier, 16) {}

bool TurtleLayEggGoal::canUse(MobContext& mob) {
    return state(mob).turtleHasEgg && closerToHome(mob, kHomeLayRange) && MoveToBlockGoal::canUse(mob);
}

bool TurtleLayEggGoal::canContinueToUse(MobContext& mob) {
    return MoveToBlockGoal::canContinueToUse(mob) && state(mob).turtleHasEgg && closerToHome(mob, kHomeLayRange);
}

void TurtleLayEggGoal::tick(MobContext& mob) {
    MoveToBlockGoal::tick(mob);
    mobMisc::State& held = state(mob);
    if (!inWater(mob.world, mob.mob) && reachedTarget()) {
        if (held.turtleLayCounter < 1) {
            setLayingEgg(mob, true);
        } else if (held.turtleLayCounter > adjustedTickDelay(kLayTicks)) {
            const Block& target = targetBlock();
            mob.actions.layTurtleEgg(mob.mob, target[0], target[1] + 1, target[2], mob.random.nextInt(4) + 1);
            held.turtleHasEgg = false;
            setLayingEgg(mob, false);
            mob.mob.loveTicks = kLayLoveTicks;
        }
        if (held.turtleLaying) {
            ++held.turtleLayCounter;
        }
    }
}

bool TurtleLayEggGoal::isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    return isAir(mob, x, y + 1, z) && isSand(mob, x, y, z);
}

TurtleGoToWaterGoal::TurtleGoToWaterGoal(double speedModifier)
    : MoveToBlockGoal(speedModifier, 24), speedModifier_(speedModifier) {
    verticalSearchStart_ = -1;
}

bool TurtleGoToWaterGoal::canUse(MobContext& mob) {
    setSpeedModifier(isBaby(mob) ? 2.0 : speedModifier_);
    const mobMisc::State& held = state(mob);
    if (isBaby(mob) && !inWater(mob.world, mob.mob)) {
        return MoveToBlockGoal::canUse(mob);
    }
    return !held.turtleGoingHome && !inWater(mob.world, mob.mob) && !held.turtleHasEgg && MoveToBlockGoal::canUse(mob);
}

bool TurtleGoToWaterGoal::canContinueToUse(MobContext& mob) {
    const Block& target = targetBlock();
    return !inWater(mob.world, mob.mob) && tryTicks() <= kWaterGiveUpTicks &&
           isValidTarget(mob, target[0], target[1], target[2]);
}

bool TurtleGoToWaterGoal::shouldRecalculatePath() const { return tryTicks() % 160 == 0; }

bool TurtleGoToWaterGoal::isValidTarget(MobContext& mob, std::int32_t x, std::int32_t y, std::int32_t z) {
    return pathAt(mob, x, y, z) == "water";
}

bool TurtleGoHomeGoal::canUse(MobContext& mob) {
    if (isBaby(mob)) {
        return false;
    }
    if (state(mob).turtleHasEgg) {
        return true;
    }
    if (mob.random.nextInt(reducedTickDelay(700)) != 0) {
        return false;
    }
    return !closerToHome(mob, kFarFromHome);
}

bool TurtleGoHomeGoal::canContinueToUse(MobContext& mob) {
    return !closerToHome(mob, kNearHome) && !stuck_ && closeToHomeTryTicks_ <= adjustedTickDelay(kGiveUpTicks);
}

void TurtleGoHomeGoal::start(MobContext& mob) {
    state(mob).turtleGoingHome = true;
    stuck_ = false;
    closeToHomeTryTicks_ = 0;
}

void TurtleGoHomeGoal::stop(MobContext& mob) { state(mob).turtleGoingHome = false; }

void TurtleGoHomeGoal::tick(MobContext& mob) {
    const Block home = homeOf(mob);
    const bool closeToHome = closerToHome(mob, kWithinHome);
    if (closeToHome) {
        ++closeToHomeTryTicks_;
    }
    if (!navigation::isDone(mob.mob)) {
        return;
    }
    const randomPos::Spot target = bottomCenter(home);
    std::optional<randomPos::Spot> next =
        randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 16, 3, target, towardsAngle(10.0));
    if (!next.has_value()) {
        next = randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 8, 7, target,
                                            static_cast<double>(static_cast<float>(std::numbers::pi) / 2.0F));
    }
    if (next.has_value() && !closeToHome &&
        pathAt(mob, static_cast<std::int32_t>(std::floor((*next)[0])),
               static_cast<std::int32_t>(std::floor((*next)[1])),
               static_cast<std::int32_t>(std::floor((*next)[2]))) != "water") {
        next = randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 16, 5, target,
                                            static_cast<double>(static_cast<float>(std::numbers::pi) / 2.0F));
    }
    if (!next.has_value()) {
        stuck_ = true;
        return;
    }
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, (*next)[0], (*next)[1], (*next)[2], speedModifier_));
}

bool TurtleTravelGoal::canUse(MobContext& mob) {
    const mobMisc::State& held = state(mob);
    return !held.turtleGoingHome && !held.turtleHasEgg && inWater(mob.world, mob.mob);
}

bool TurtleTravelGoal::canContinueToUse(MobContext& mob) {
    const mobMisc::State& held = state(mob);
    return !navigation::isDone(mob.mob) && !stuck_ && !held.turtleGoingHome && mob.mob.loveTicks <= 0 &&
           !held.turtleHasEgg;
}

void TurtleTravelGoal::start(MobContext& mob) {
    const std::int32_t dx = mob.random.nextInt((2 * kTravelSpan) + 1) - kTravelSpan;
    std::int32_t dy = mob.random.nextInt((2 * kTravelDepth) + 1) - kTravelDepth;
    const std::int32_t dz = mob.random.nextInt((2 * kTravelSpan) + 1) - kTravelSpan;
    if (static_cast<double>(dy) + mob.mob.y > static_cast<double>(mob.seaLevel - 1)) {
        dy = 0;
    }
    state(mob).turtleTravel = Block{static_cast<std::int32_t>(std::floor(dx + mob.mob.x)),
                                    static_cast<std::int32_t>(std::floor(dy + mob.mob.y)),
                                    static_cast<std::int32_t>(std::floor(dz + mob.mob.z))};
    stuck_ = false;
}

void TurtleTravelGoal::stop(MobContext& mob) { state(mob).turtleTravel.reset(); }

void TurtleTravelGoal::tick(MobContext& mob) {
    const std::optional<Block>& travel = state(mob).turtleTravel;
    if (!travel.has_value()) {
        stuck_ = true;
        return;
    }
    if (!navigation::isDone(mob.mob)) {
        return;
    }
    const randomPos::Spot target = bottomCenter(*travel);
    std::optional<randomPos::Spot> next =
        randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 16, 3, target, towardsAngle(10.0));
    if (!next.has_value()) {
        next = randomPos::defaultPosTowards(mob.world, mob.mob, mob.random, 8, 7, target,
                                            static_cast<double>(static_cast<float>(std::numbers::pi) / 2.0F));
    }
    if (next.has_value()) {
        const auto cx = static_cast<std::int32_t>(std::floor((*next)[0]));
        const auto cz = static_cast<std::int32_t>(std::floor((*next)[2]));
        const auto y = static_cast<std::int32_t>(std::floor((*next)[1]));
        if (!mob.world.blockState(cx - kChunkMargin, y, cz - kChunkMargin).has_value() ||
            !mob.world.blockState(cx + kChunkMargin, y, cz + kChunkMargin).has_value()) {
            next.reset();
        }
    }
    if (!next.has_value()) {
        stuck_ = true;
        return;
    }
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, (*next)[0], (*next)[1], (*next)[2], speedModifier_));
}

bool TurtleRandomStrollGoal::canUse(MobContext& mob) {
    const mobMisc::State& held = state(mob);
    return !inWater(mob.world, mob.mob) && !held.turtleGoingHome && !held.turtleHasEgg && RandomStrollGoal::canUse(mob);
}

} // namespace basalt::server::runtime::entity::ai
