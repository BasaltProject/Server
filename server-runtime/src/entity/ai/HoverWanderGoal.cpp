#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/FlyingGoals.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <numbers>
#include <optional>

namespace basalt::server::runtime::entity::ai {
namespace {

using Block = std::array<std::int32_t, 3>;

namespace mth = wire::common::mth;

constexpr std::int32_t kInterval = 120;
constexpr std::int32_t kAttempts = 10;
constexpr std::int32_t kHorizontal = 8;
constexpr std::int32_t kHoverVertical = 7;
constexpr std::int32_t kAirVertical = 4;
constexpr std::int32_t kAirLift = -2;
constexpr std::int32_t kHoverMax = 3;
constexpr std::int32_t kHoverMin = 1;
constexpr float kLookSpeed = 10.0F;

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

/// parity: `RandomPos.generateRandomPos`: ten rolls, all of them made, the first best one kept.
template <typename Supplier>
[[nodiscard]] std::optional<Block> bestOf(Supplier supply) {
    std::optional<Block> best;
    for (std::int32_t attempt = 0; attempt < kAttempts; ++attempt) {
        const std::optional<Block> candidate = supply();
        if (candidate.has_value() && !best.has_value()) {
            best = candidate;
        }
    }
    return best;
}

} // namespace

HoverWanderGoal::HoverWanderGoal(double step) : step_(step) {}

bool HoverWanderGoal::canUse(MobContext& mob) {
    if (mob.random.nextInt(reducedTickDelay(kInterval)) != 0) {
        return false;
    }
    // parity: `Entity.getViewVector(0)`, the way its body faces.
    const float yaw = -mob.mob.yaw * (std::numbers::pi_v<float> / 180.0F);
    const float pitch = mob.mob.pitch * (std::numbers::pi_v<float> / 180.0F);
    const auto xDir = static_cast<double>(mth::sin(yaw) * mth::cos(pitch));
    const auto zDir = static_cast<double>(mth::cos(yaw) * mth::cos(pitch));
    std::optional<Block> target = bestOf([&] { return hoverCandidate(mob, xDir, zDir); });
    if (!target.has_value()) {
        target = bestOf([&] { return airCandidate(mob, xDir, zDir); });
    }
    if (!target.has_value()) {
        return false;
    }
    // parity: `Vec3.atBottomCenterOf`.
    wanted_ = {(*target)[0] + 0.5, static_cast<double>((*target)[1]), (*target)[2] + 0.5};
    arrived_ = false;
    return true;
}

bool HoverWanderGoal::canContinueToUse(MobContext& /*mob*/) { return !arrived_; }

void HoverWanderGoal::tick(MobContext& mob) {
    if (!flyingMovement::approach(mob.mob, wanted_[0], wanted_[1], wanted_[2], step_)) {
        arrived_ = true;
    }
    lookControl::lookAt(mob.mob, wanted_[0], wanted_[1], wanted_[2], kLookSpeed);
}

} // namespace basalt::server::runtime::entity::ai
