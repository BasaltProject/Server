#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/FlyingPathFinder.hpp>
#include <basalt/server/runtime/entity/PathTypes.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/BeeNavigation.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace basalt::server::runtime::entity::ai::beeNavigation {
namespace {

namespace mth = wire::common::mth;
using pathTypes::PathType;

constexpr std::int32_t kAttempts = 10;
constexpr double kQuarterTurn = static_cast<double>(std::numbers::pi_v<float> / 2.0F);

[[nodiscard]] bool inWorld(const world::WorldState& world, const Block& at) {
    return world.blockState(at[0], at[1], at[2]).has_value();
}

/// parity: `GoalUtils.isSolid`, `BlockState.isSolid`.
[[nodiscard]] bool solid(const world::WorldState& world, const Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() && registry::blockShapes::isSolidState(*state);
}

[[nodiscard]] bool water(const world::WorldState& world, const Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

[[nodiscard]] bool air(const world::WorldState& world, const Block& at) {
    const std::optional<std::int32_t> state = world.blockState(at[0], at[1], at[2]);
    return state.has_value() && registry::blocks::isAir(*state);
}

/// parity: `WalkNodeEvaluator.getPathTypeStatic`.
[[nodiscard]] PathType staticType(const MobContext& mob, const Block& at) {
    const PathType own = pathTypes::fromState(mob.world, at[0], at[1], at[2]);
    if (own != PathType::Open || at[1] < mob.minY + 1) {
        return own;
    }
    switch (pathTypes::fromState(mob.world, at[0], at[1] - 1, at[2])) {
    case PathType::Open:
    case PathType::Water:
    case PathType::Lava:
    case PathType::Walkable:
        return PathType::Open;
    case PathType::Fire:
        return PathType::Fire;
    case PathType::Damaging:
        return PathType::Damaging;
    case PathType::StickyHoney:
        return PathType::StickyHoney;
    case PathType::PowderSnow:
        return PathType::OnTopOfPowderSnow;
    case PathType::DamageCautious:
        return PathType::DamageCautious;
    case PathType::Trapdoor:
        return PathType::OnTopOfTrapdoor;
    default:
        return pathTypes::checkNeighbours(mob.world, at[0], at[1], at[2], PathType::Walkable);
    }
}

/// parity: `GoalUtils.hasMalus`.
[[nodiscard]] bool hasMalus(const MobContext& mob, const Block& at) {
    return pathTypes::malusOf(pathTypes::of(mob.mob.type()), staticType(mob, at)) != 0.0F;
}

/// parity: `RandomPos.generateRandomDirectionWithinRadians` with no minimum distance.
[[nodiscard]] std::optional<Block> direction(random::RandomSource& random, std::int32_t horizontal,
                                             std::int32_t vertical, std::int32_t lift, double xDir, double zDir,
                                             double spread) {
    const double centre = std::atan2(zDir, xDir) - kQuarterTurn;
    const double yaw = centre + (static_cast<double>((2.0F * random.nextFloat()) - 1.0F) * spread);
    const double distance =
        std::sqrt(random.nextDouble()) * horizontal * static_cast<double>(std::numbers::sqrt2_v<float>);
    const double xt = -distance * std::sin(yaw);
    const double zt = distance * std::cos(yaw);
    if (std::abs(xt) > horizontal || std::abs(zt) > horizontal) {
        return std::nullopt;
    }
    const std::int32_t yt = random.nextInt((2 * vertical) + 1) - vertical + lift;
    return Block{mth::floor(xt), yt, mth::floor(zt)};
}

[[nodiscard]] Block toward(const ServerEntity& bee, const Block& offset) {
    return Block{mth::floor(offset[0] + bee.x), mth::floor(offset[1] + bee.y), mth::floor(offset[2] + bee.z)};
}

/// parity: `AirAndWaterRandomPos.generateRandomPos`.
[[nodiscard]] std::optional<Block> airCandidate(MobContext& mob, std::int32_t horizontal, std::int32_t vertical,
                                                std::int32_t lift, double xDir, double zDir, double spread) {
    const std::optional<Block> offset = direction(mob.random, horizontal, vertical, lift, xDir, zDir, spread);
    if (!offset.has_value()) {
        return std::nullopt;
    }
    Block at = toward(mob.mob, *offset);
    if (!inWorld(mob.world, at)) {
        return std::nullopt;
    }
    // parity: `RandomPos.moveUpOutOfSolid`.
    while (solid(mob.world, at) && inWorld(mob.world, at)) {
        ++at[1];
    }
    return hasMalus(mob, at) ? std::nullopt : std::optional{at};
}

/// parity: `HoverRandomPos.getPos`'s supplier, with the bee's `isStableDestination`.
[[nodiscard]] std::optional<Block> hoverCandidate(MobContext& mob, double xDir, double zDir) {
    constexpr std::int32_t kHorizontal = 8;
    constexpr std::int32_t kVertical = 7;
    constexpr std::int32_t kHoverMax = 3;
    constexpr std::int32_t kHoverMin = 1;
    const std::optional<Block> offset = direction(mob.random, kHorizontal, kVertical, 0, xDir, zDir, kQuarterTurn);
    if (!offset.has_value()) {
        return std::nullopt;
    }
    Block at = toward(mob.mob, *offset);
    const Block below{at[0], at[1] - 1, at[2]};
    if (!inWorld(mob.world, at) || air(mob.world, below)) {
        return std::nullopt;
    }
    const std::int32_t hover = mob.random.nextInt(kHoverMax - kHoverMin + 1) + kHoverMin;
    // parity: `RandomPos.moveUpToAboveSolid`.
    if (solid(mob.world, at)) {
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
    }
    return water(mob.world, at) || hasMalus(mob, at) ? std::nullopt : std::optional{at};
}

/// parity: `RandomPos.generateRandomPos` weighed by `Bee.getWalkTargetValue`, ten for air.
template <typename Supplier>
[[nodiscard]] std::optional<Block> bestOf(MobContext& mob, Supplier supply) {
    std::optional<Block> best;
    double bestWeight = -std::numeric_limits<double>::infinity();
    for (std::int32_t attempt = 0; attempt < kAttempts; ++attempt) {
        const std::optional<Block> candidate = supply();
        if (!candidate.has_value()) {
            continue;
        }
        const double weight = air(mob.world, *candidate) ? 10.0 : 0.0;
        if (weight > bestWeight) {
            bestWeight = weight;
            best = candidate;
        }
    }
    return best;
}

[[nodiscard]] std::int32_t manhattan(const Block& a, const Block& b) {
    return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
}

} // namespace

Route plan(MobContext& mob, const Block& target, std::int32_t accuracy, float visitedMultiplier) {
    // parity: `getMaxPathLength`, the follow range or the bee's required forty-eight.
    const double range =
        std::max(mob.mob.attributeWithModifiers("minecraft:follow_range"), static_cast<double>(bees::kPathLength));
    const auto maxNodes = static_cast<std::int32_t>(static_cast<float>(mth::floor(static_cast<float>(range) * 16.0F)) *
                                                    visitedMultiplier);
    const flyingPathFinder::Footprint size{.width = static_cast<std::int32_t>(std::floor(mob.mob.width() + 1.0)),
                                           .height = static_cast<std::int32_t>(std::floor(mob.mob.height() + 1.0))};
    const Block from{mth::floor(mob.mob.x), mth::floor(mob.mob.y + 0.5), mth::floor(mob.mob.z)};
    const flyingPathFinder::Mob flier{.maluses = pathTypes::of(mob.mob.type()),
                                      .at = Block{mth::floor(mob.mob.x), mth::floor(mob.mob.y), mth::floor(mob.mob.z)},
                                      .minY = mob.minY};
    Route route{.nodes = flyingPathFinder::find(mob.world, size, from, target, maxNodes, range, flier),
                .target = target};
    route.reaches = manhattan(route.nodes.empty() ? from : route.nodes.back(), target) <= accuracy;
    return route;
}

bool follow(MobContext& mob, const Route& route, double speed) {
    return navigation::follow(mob.mob, route.nodes,
                              {route.target[0] + 0.5, static_cast<double>(route.target[1]), route.target[2] + 0.5},
                              speed);
}

void hopTowards(MobContext& mob, const Block& target) {
    const Block own{mth::floor(mob.mob.x), mth::floor(mob.mob.y), mth::floor(mob.mob.z)};
    // parity: `Vec3.atBottomCenterOf(target)`, whose height is the block's own.
    const std::int32_t yDelta = target[1] - own[1];
    const std::int32_t lift = yDelta > 2 ? 4 : (yDelta < -2 ? -4 : 0);
    std::int32_t horizontal = 6;
    std::int32_t vertical = 8;
    if (const std::int32_t distance = manhattan(own, target); distance < 15) {
        horizontal = distance / 2;
        vertical = distance / 2;
    }
    // parity: `AirRandomPos.getPosTowards`, never into water, within a tenth of a half turn.
    const double xDir = (target[0] + 0.5) - mob.mob.x;
    const double zDir = (target[2] + 0.5) - mob.mob.z;
    const double spread = static_cast<double>(std::numbers::pi_v<float> / 10.0F);
    const std::optional<Block> spot = bestOf(mob, [&]() -> std::optional<Block> {
        const std::optional<Block> at = airCandidate(mob, horizontal, vertical, lift, xDir, zDir, spread);
        return at.has_value() && !water(mob.world, *at) ? at : std::nullopt;
    });
    if (!spot.has_value()) {
        return;
    }
    // parity: `setMaxVisitedNodesMultiplier(0.5)` then `moveTo` the spot's bottom centre.
    static_cast<void>(follow(mob, plan(mob, *spot, 1, 0.5F), 1.0));
}

std::optional<std::array<double, 3>> wanderSpot(MobContext& mob, const std::optional<Block>& hive,
                                                std::int32_t threshold) {
    double xDir = 0.0;
    double zDir = 0.0;
    if (hive.has_value() && !bees::closerThan(mob.mob, *hive, threshold)) {
        // parity: `Vec3.atCenterOf(hive).subtract(position()).normalize()`.
        const double dx = (*hive)[0] + 0.5 - mob.mob.x;
        const double dy = (*hive)[1] + 0.5 - mob.mob.y;
        const double dz = (*hive)[2] + 0.5 - mob.mob.z;
        const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (length >= 1.0E-5) {
            xDir = dx / length;
            zDir = dz / length;
        }
    } else {
        // parity: `getViewVector(0)`.
        const float yaw = -mob.mob.yaw * (std::numbers::pi_v<float> / 180.0F);
        const float pitch = mob.mob.pitch * (std::numbers::pi_v<float> / 180.0F);
        xDir = static_cast<double>(mth::sin(yaw) * mth::cos(pitch));
        zDir = static_cast<double>(mth::cos(yaw) * mth::cos(pitch));
    }
    std::optional<Block> spot = bestOf(mob, [&] { return hoverCandidate(mob, xDir, zDir); });
    if (!spot.has_value()) {
        spot = bestOf(mob, [&] { return airCandidate(mob, 8, 4, -2, xDir, zDir, kQuarterTurn); });
    }
    if (!spot.has_value()) {
        return std::nullopt;
    }
    return std::array<double, 3>{(*spot)[0] + 0.5, static_cast<double>((*spot)[1]), (*spot)[2] + 0.5};
}

} // namespace basalt::server::runtime::entity::ai::beeNavigation
