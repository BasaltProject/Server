#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/DolphinGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace basalt::server::runtime::entity::ai {
namespace {

using Position = std::array<std::int32_t, 3>;

/// parity: `BreathAirGoal.canUse`'s 140.
constexpr std::int32_t kLowAir = 140;
constexpr std::int32_t kAirSearchHeight = 8;
constexpr double kDegrees = 180.0 / std::numbers::pi;

[[nodiscard]] std::int32_t floorOf(double value) { return static_cast<std::int32_t>(std::floor(value)); }

[[nodiscard]] Position blockPosition(const ServerEntity& mob) {
    return {floorOf(mob.x), floorOf(mob.y), floorOf(mob.z)};
}

[[nodiscard]] bool holdsWaterAt(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && swimPath::holdsWater(*state);
}

/// parity: `DefaultRandomPos.getPos(mob, 10, 7)`: ten rolls, the first square that is neither solid
/// nor blocked kept, since a swimmer weighs every target the same.
[[nodiscard]] std::optional<Position> defaultRandomPos(MobContext& mob, std::int32_t horizontal,
                                                       std::int32_t vertical) {
    const Position at = blockPosition(mob.mob);
    std::optional<Position> found;
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const std::int32_t dx = mob.random.nextInt((2 * horizontal) + 1) - horizontal;
        const std::int32_t dy = mob.random.nextInt((2 * vertical) + 1) - vertical;
        const std::int32_t dz = mob.random.nextInt((2 * horizontal) + 1) - horizontal;
        const Position candidate{at[0] + dx, at[1] + dy, at[2] + dz};
        const std::optional<std::int32_t> state = mob.world.blockState(candidate[0], candidate[1], candidate[2]);
        if (found.has_value() || !state.has_value() || !registry::blockShapes::isEmptyShape(*state)) {
            continue;
        }
        found = candidate;
    }
    return found;
}

/// parity: `BreathAirGoal.givesAir`: dry or a bubble column, and nothing to bump into.
[[nodiscard]] bool givesAir(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    if (!state.has_value() || !registry::blockShapes::isEmptyShape(*state)) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
    const bool bubbles = block != nullptr && block->path() == "bubble_column";
    return !swimPath::holdsWater(*state) || bubbles;
}

} // namespace

bool BreathAirGoal::canUse(MobContext& mob) { return mob.mob.airSupply < kLowAir; }

void BreathAirGoal::start(MobContext& mob) {
    navigation::stop(mob.mob);
    findAirPosition(mob);
}

void BreathAirGoal::tick(MobContext& mob) {
    findAirPosition(mob);
    // parity: the goal's own extra push, with the input the move control left last tick.
    livingMovement::swimStep(mob.world, mob.mob, mob.mob.swimForward, mob.mob.swimUp, static_cast<double>(0.02F));
}

void BreathAirGoal::findAirPosition(MobContext& mob) {
    const Position at = blockPosition(mob.mob);
    Position destination{at[0], floorOf(mob.mob.y + kAirSearchHeight), at[2]};
    for (std::int32_t y = at[1]; y <= at[1] + kAirSearchHeight; ++y) {
        if (givesAir(mob.world, at[0], y, at[2])) {
            destination[1] = y;
            break;
        }
    }
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, destination[0], destination[1] + 1, destination[2], 1.0));
}

bool TryFindWaterGoal::canUse(MobContext& mob) {
    const Position at = blockPosition(mob.mob);
    return mob.mob.onGround && !holdsWaterAt(mob.world, at[0], at[1], at[2]);
}

void TryFindWaterGoal::start(MobContext& mob) {
    // parity: `BlockPos.betweenClosed`, x innermost, from two below up to its own height.
    const std::int32_t minX = floorOf(mob.mob.x - 2.0);
    const std::int32_t minZ = floorOf(mob.mob.z - 2.0);
    const std::int32_t top = floorOf(mob.mob.y);
    for (std::int32_t z = minZ; z <= floorOf(mob.mob.z + 2.0); ++z) {
        for (std::int32_t y = floorOf(mob.mob.y - 2.0); y <= top; ++y) {
            for (std::int32_t x = minX; x <= floorOf(mob.mob.x + 2.0); ++x) {
                if (holdsWaterAt(mob.world, x, y, z)) {
                    moveControl::setWanted(mob.mob, x, y, z, 1.0);
                    return;
                }
            }
        }
    }
}

RandomSwimmingGoal::RandomSwimmingGoal(double speedModifier, std::int32_t interval)
    : speedModifier_(speedModifier), interval_(interval) {}

bool RandomSwimmingGoal::canUse(MobContext& mob) {
    if (mob.mob.idleTicks >= 100 || mob.random.nextInt(reducedTickDelay(interval_)) != 0) {
        return false;
    }
    // parity: `getRandomSwimmablePos`, ten more tries while the pick is not open water.
    std::optional<Position> target = defaultRandomPos(mob, 10, 7);
    for (std::int32_t count = 0; target.has_value() &&
                                 !swimPath::isSwimmable(mob.world, (*target)[0], (*target)[1], (*target)[2]) &&
                                 count++ < 10;) {
        target = defaultRandomPos(mob, 10, 7);
    }
    if (!target.has_value()) {
        return false;
    }
    wanted_ = {(*target)[0] + 0.5, static_cast<double>((*target)[1]), (*target)[2] + 0.5};
    return true;
}

bool RandomSwimmingGoal::canContinueToUse(MobContext& mob) { return !navigation::isDone(mob.mob); }

void RandomSwimmingGoal::start(MobContext& mob) {
    static_cast<void>(navigation::moveTo(mob.world, mob.mob, wanted_[0], wanted_[1], wanted_[2], speedModifier_));
}

void RandomSwimmingGoal::stop(MobContext& mob) { navigation::stop(mob.mob); }

DolphinJumpGoal::DolphinJumpGoal(std::int32_t interval) : interval_((interval + 1) / 2) {}

namespace {

/// parity: `Entity.getMotionDirection` over `Direction.getApproximateNearest`: the face with the
/// largest positive dot, north when nothing moves. Only the horizontal step matters here.
[[nodiscard]] std::array<std::int32_t, 2> motionSteps(const ServerEntity& mob) {
    constexpr std::array<std::array<std::int32_t, 3>, 6> kFaces{
        {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}}};
    const auto x = static_cast<float>(mob.velocityX);
    const auto y = static_cast<float>(mob.velocityY);
    const auto z = static_cast<float>(mob.velocityZ);
    std::array<std::int32_t, 3> best{0, 0, -1};
    float highest = std::numeric_limits<float>::denorm_min();
    for (const auto& face : kFaces) {
        const float dot =
            (x * static_cast<float>(face[0])) + (y * static_cast<float>(face[1])) + (z * static_cast<float>(face[2]));
        if (dot > highest) {
            highest = dot;
            best = face;
        }
    }
    return {best[0], best[2]};
}

} // namespace

bool DolphinJumpGoal::canUse(MobContext& mob) {
    if (mob.random.nextInt(interval_) != 0) {
        return false;
    }
    const auto [stepX, stepZ] = motionSteps(mob.mob);
    const Position at = blockPosition(mob.mob);
    for (const std::int32_t step : {0, 1, 4, 5, 6, 7}) {
        const std::int32_t x = at[0] + (stepX * step);
        const std::int32_t z = at[2] + (stepZ * step);
        const bool water = swimPath::isSwimmable(mob.world, x, at[1], z);
        const auto isAir = [&mob, x, z](std::int32_t y) {
            const std::optional<std::int32_t> state = mob.world.blockState(x, y, z);
            return state.has_value() && registry::blocks::isAir(*state);
        };
        if (!water || !isAir(at[1] + 1) || !isAir(at[1] + 2)) {
            return false;
        }
    }
    return true;
}

bool DolphinJumpGoal::canContinueToUse(MobContext& mob) {
    const double vy = mob.mob.velocityY;
    const bool settled = vy * vy < static_cast<double>(0.03F) && mob.mob.pitch != 0.0F &&
                         std::abs(mob.mob.pitch) < 10.0F && inWater(mob.world, mob.mob);
    return !settled && !mob.mob.onGround;
}

void DolphinJumpGoal::start(MobContext& mob) {
    const auto [stepX, stepZ] = motionSteps(mob.mob);
    mob.mob.velocityX += stepX * 0.6;
    mob.mob.velocityY += 0.7;
    mob.mob.velocityZ += stepZ * 0.6;
    navigation::stop(mob.mob);
}

void DolphinJumpGoal::stop(MobContext& mob) { mob.mob.pitch = 0.0F; }

void DolphinJumpGoal::tick(MobContext& mob) {
    if (!breached_) {
        const Position at = blockPosition(mob.mob);
        breached_ = holdsWaterAt(mob.world, at[0], at[1], at[2]);
    }
    const double vx = mob.mob.velocityX;
    const double vy = mob.mob.velocityY;
    const double vz = mob.mob.velocityZ;
    if (vy * vy < static_cast<double>(0.03F) && mob.mob.pitch != 0.0F) {
        // parity: `Mth.rotLerp(0.2F, pitch, 0)`.
        mob.mob.pitch += 0.2F * moveControl::wrapDegrees(0.0F - mob.mob.pitch);
    } else if (std::sqrt((vx * vx) + (vy * vy) + (vz * vz)) > static_cast<double>(1.0E-5F)) {
        mob.mob.pitch = static_cast<float>(std::atan2(-vy, std::sqrt((vx * vx) + (vz * vz))) * kDegrees);
    }
}

DolphinSwimWithPlayerGoal::DolphinSwimWithPlayerGoal(double speedModifier) : speedModifier_(speedModifier) {}

bool DolphinSwimWithPlayerGoal::canUse(MobContext& mob) {
    // parity: `forNonCombat().range(10)`, the nearest player whatever it is doing.
    const PlayerView* nearest = mob.nearestPlayer(10.0);
    player_ = nearest != nullptr ? std::optional{nearest->entityId} : std::nullopt;
    return nearest != nullptr && nearest->swimming && mob.mob.targetId != nearest->entityId;
}

bool DolphinSwimWithPlayerGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* player = player_.has_value() ? mob.playerById(*player_) : nullptr;
    return player != nullptr && player->swimming && mob.distanceSquaredTo(player->x, player->y, player->z) < 256.0;
}

void DolphinSwimWithPlayerGoal::start(MobContext& mob) {
    if (player_.has_value()) {
        mob.actions.applyEffect(*player_, "dolphins_grace", 100, 0);
    }
}

void DolphinSwimWithPlayerGoal::stop(MobContext& mob) {
    player_.reset();
    navigation::stop(mob.mob);
}

void DolphinSwimWithPlayerGoal::tick(MobContext& mob) {
    const PlayerView* player = player_.has_value() ? mob.playerById(*player_) : nullptr;
    if (player == nullptr) {
        return;
    }
    // parity: `getMaxHeadYRot() + 20` and `getMaxHeadXRot()`, twenty-one and one for a dolphin.
    lookControl::lookAt(mob.mob, player->x, player->y + (player->height * 0.85), player->z, 21.0F, 1.0F);
    if (mob.distanceSquaredTo(player->x, player->y, player->z) < 6.25) {
        navigation::stop(mob.mob);
    } else {
        static_cast<void>(navigation::moveTo(mob.world, mob.mob, player->x, player->y, player->z, speedModifier_));
    }
    if (player->swimming && mob.random.nextInt(6) == 0) {
        mob.actions.applyEffect(*player_, "dolphins_grace", 100, 0);
    }
}

} // namespace basalt::server::runtime::entity::ai
