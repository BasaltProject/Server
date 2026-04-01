#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/FlyingPathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/FlyingGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace basalt::server::runtime::entity::ai {
namespace {

using Block = std::array<std::int32_t, 3>;

namespace mth = wire::common::mth;

constexpr std::int32_t kAttempts = 10;
constexpr std::int32_t kHorizontal = 8;
constexpr std::int32_t kHoverVertical = 7;
constexpr std::int32_t kAirVertical = 4;
constexpr std::int32_t kAirLift = -2;
constexpr std::int32_t kHoverMax = 3;
constexpr std::int32_t kHoverMin = 1;

/// parity: `BlockState.isSolid` as `GoalUtils.isSolid` asks it; a square past the world is treated
/// as open, which only stops the climb.
[[nodiscard]] bool solid(const world::WorldState& world, const Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() && !registry::blockShapes::isEmptyShape(*state);
}

[[nodiscard]] bool inWorld(const world::WorldState& world, const Block& at) {
    return world.blockState(at[0], at[1], at[2]).has_value();
}

[[nodiscard]] bool water(const world::WorldState& world, const Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

/// parity: `RandomPos.generateRandomDirectionWithinRadians` with no minimum distance.
[[nodiscard]] std::optional<Block> directionWithin(random::RandomSource& random, std::int32_t vertical,
                                                   std::int32_t lift, double xDir, double zDir) {
    constexpr double kSpread = static_cast<double>(std::numbers::pi_v<float> / 2.0F);
    const double centre = std::atan2(zDir, xDir) - static_cast<double>(std::numbers::pi_v<float> / 2.0F);
    const double yaw = centre + (static_cast<double>((2.0F * random.nextFloat()) - 1.0F) * kSpread);
    const double distance =
        std::sqrt(random.nextDouble()) * kHorizontal * static_cast<double>(std::numbers::sqrt2_v<float>);
    const double xt = -distance * std::sin(yaw);
    const double zt = distance * std::cos(yaw);
    if (std::abs(xt) > kHorizontal || std::abs(zt) > kHorizontal) {
        return std::nullopt;
    }
    const std::int32_t yt = random.nextInt((2 * vertical) + 1) - vertical + lift;
    return Block{mth::floor(xt), yt, mth::floor(zt)};
}

[[nodiscard]] Block toward(const ServerEntity& mob, const Block& direction) {
    return Block{mth::floor(direction[0] + mob.x), mth::floor(direction[1] + mob.y), mth::floor(direction[2] + mob.z)};
}

/// parity: `HoverRandomPos.getPos`'s supplier.
[[nodiscard]] std::optional<Block> hoverCandidate(MobContext& mob, double xDir, double zDir) {
    const std::optional<Block> direction = directionWithin(mob.random, kHoverVertical, 0, xDir, zDir);
    if (!direction.has_value()) {
        return std::nullopt;
    }
    Block at = toward(mob.mob, *direction);
    // parity: `FlyingPathNavigation.isStableDestination`, something to stand on in that very square.
    if (!inWorld(mob.world, at) || !solid(mob.world, at)) {
        return std::nullopt;
    }
    const std::int32_t hover = mob.random.nextInt(kHoverMax - kHoverMin + 1) + kHoverMin;
    // parity: `RandomPos.moveUpToAboveSolid`.
    ++at[1];
    while (inWorld(mob.world, at) && solid(mob.world, at)) {
        ++at[1];
    }
    const std::int32_t open = at[1];
    while (inWorld(mob.world, at) && at[1] - open < hover) {
        ++at[1];
        if (solid(mob.world, at)) {
            --at[1];
            break;
        }
    }
    return water(mob.world, at) ? std::nullopt : std::optional{at};
}

/// parity: `AirAndWaterRandomPos.generateRandomPos`.
[[nodiscard]] std::optional<Block> airCandidate(MobContext& mob, double xDir, double zDir) {
    const std::optional<Block> direction = directionWithin(mob.random, kAirVertical, kAirLift, xDir, zDir);
    if (!direction.has_value()) {
        return std::nullopt;
    }
    Block at = toward(mob.mob, *direction);
    if (!inWorld(mob.world, at)) {
        return std::nullopt;
    }
    // parity: `RandomPos.moveUpOutOfSolid`.
    while (solid(mob.world, at) && inWorld(mob.world, at)) {
        ++at[1];
    }
    return at;
}

/// parity: `Monster.getWalkTargetValue`, less the brighter the square:
/// `-(getLightLevelDependentMagicValue(pos) - 0.5)`.
[[nodiscard]] float walkTargetValue(MobContext& mob, const Block& at) {
    const auto v = static_cast<float>(world::worldLight::rawBrightness(mob.world, at[0], at[1], at[2], mob.hasSkylight,
                                                                       mob.skyDarken)) /
                   15.0F;
    const float curved = v / (4.0F - (3.0F * v));
    return -((mob.ambientLight + ((1.0F - mob.ambientLight) * curved)) - 0.5F);
}

/// parity: `RandomPos.generateRandomPos`: ten rolls, all of them made, the first of the best kept.
template <typename Supplier>
[[nodiscard]] std::optional<Block> bestOf(MobContext& mob, Supplier supply) {
    std::optional<Block> best;
    float bestWeight = -std::numeric_limits<float>::infinity();
    for (std::int32_t attempt = 0; attempt < kAttempts; ++attempt) {
        const std::optional<Block> candidate = supply();
        if (!candidate.has_value()) {
            continue;
        }
        const float weight = walkTargetValue(mob, *candidate);
        if (weight > bestWeight) {
            bestWeight = weight;
            best = candidate;
        }
    }
    return best;
}

} // namespace

HoverWanderGoal::HoverWanderGoal(std::int32_t interval, bool whenIdle) : interval_(interval), whenIdle_(whenIdle) {}

bool HoverWanderGoal::canUse(MobContext& mob) {
    if (whenIdle_ ? (!navigation::isDone(mob.mob) || mob.random.nextInt(interval_) != 0)
                  : mob.random.nextInt(reducedTickDelay(interval_)) != 0) {
        return false;
    }
    // parity: `Entity.getViewVector(0)`, the way its body faces.
    const float yaw = -mob.mob.yaw * (std::numbers::pi_v<float> / 180.0F);
    const float pitch = mob.mob.pitch * (std::numbers::pi_v<float> / 180.0F);
    const auto xDir = static_cast<double>(mth::sin(yaw) * mth::cos(pitch));
    const auto zDir = static_cast<double>(mth::cos(yaw) * mth::cos(pitch));
    std::optional<Block> target = bestOf(mob, [&] { return hoverCandidate(mob, xDir, zDir); });
    if (!target.has_value()) {
        target = bestOf(mob, [&] { return airCandidate(mob, xDir, zDir); });
    }
    if (!target.has_value()) {
        return false;
    }
    // parity: `FlyingPathNavigation.createPath`, from the square half a block up its feet, as far as
    // the follow range reaches and sixteen nodes a block of it.
    const double range = mob.mob.attributeWithModifiers("minecraft:follow_range");
    const flyingPathFinder::Footprint size{.width = static_cast<std::int32_t>(std::floor(mob.mob.width() + 1.0)),
                                           .height = static_cast<std::int32_t>(std::floor(mob.mob.height() + 1.0))};
    const Block from{mth::floor(mob.mob.x), mth::floor(mob.mob.y + 0.5), mth::floor(mob.mob.z)};
    const flyingPathFinder::Mob flier{.maluses = pathTypes::of(mob.mob.type()),
                                      .at = Block{mth::floor(mob.mob.x), mth::floor(mob.mob.y), mth::floor(mob.mob.z)},
                                      .minY = mob.minY};
    std::vector<Block> path =
        flyingPathFinder::find(mob.world, size, from, *target, static_cast<std::int32_t>(range * 16.0), range, flier);
    if (path.empty()) {
        return false;
    }
    // parity: `PathNavigation.moveTo(path, 1.0)`; the flying control then follows it node by node.
    mob.mob.path = std::move(path);
    mob.mob.pathIndex = 0;
    mob.mob.walkTarget =
        std::array<double, 3>{(*target)[0] + 0.5, static_cast<double>((*target)[1]), (*target)[2] + 0.5};
    mob.mob.moveSpeed = 1.0;
    return true;
}

bool HoverWanderGoal::canContinueToUse(MobContext& mob) { return !navigation::isDone(mob.mob); }

void HoverWanderGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

} // namespace basalt::server::runtime::entity::ai
