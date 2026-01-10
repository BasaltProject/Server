#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr double kClimbSpeed = 0.2;
constexpr std::int32_t kAggroDelay = 5;
constexpr double kStareRange = 64.0;
constexpr double kCone = 0.025;
constexpr double kEyeShare = 0.85;
constexpr double kPlayerEye = 1.62;
constexpr double kDegrees = 180.0;

/// parity: sixty-four blocks across, thirty-two up or down.
constexpr double kRange = 64.0;
constexpr std::int32_t kSearchLimit = 64;
constexpr float kWaterDamage = 1.0F;
constexpr std::int32_t kTeleportOnHurtOdds = 5;
constexpr std::int32_t kDayOdds = 30;
constexpr std::int32_t kDaylight = 8;

bool isWater(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    if (!state.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
    return block != nullptr && block->path() == "water";
}

/// parity: the teleport walks down until it finds something that blocks motion.
std::optional<double> groundBelow(const world::WorldState& world, double x, double y, double z) {
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    auto blockY = static_cast<std::int32_t>(std::floor(y));
    for (std::int32_t steps = 0; steps < kSearchLimit; ++steps) {
        const std::optional<std::int32_t> state = world.blockState(blockX, blockY - 1, blockZ);
        if (!state.has_value()) {
            return std::nullopt;
        }
        if (!registry::blockShapes::isEmptyShape(*state)) {
            const std::optional<std::int32_t> feet = world.blockState(blockX, blockY, blockZ);
            const std::optional<std::int32_t> head = world.blockState(blockX, blockY + 1, blockZ);
            if (!feet.has_value() || !head.has_value()) {
                return std::nullopt;
            }
            if (!registry::blockShapes::isEmptyShape(*feet) || !registry::blockShapes::isEmptyShape(*head)) {
                return std::nullopt;
            }
            return static_cast<double>(blockY);
        }
        blockY--;
    }
    return std::nullopt;
}

} // namespace

double distanceBetween(double ax, double ay, double az, double bx, double by, double bz) {
    const double dx = std::abs(ax - bx);
    const double dy = std::abs(ay - by);
    const double dz = std::abs(az - bz);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool ClimbWallsGoal::canUse(MobContext& mob) { return mob.mob.horizontallyBlocked; }

bool ClimbWallsGoal::canContinueToUse(MobContext& mob) { return mob.mob.horizontallyBlocked; }

void ClimbWallsGoal::tick(MobContext& mob) {
    // parity: `LivingEntity.travel`'s climbing branch, which pins the fall at 0.2 upwards.
    mob.mob.velocityY = kClimbSpeed;
    mob.mob.onGround = false;
}

/// parity: `isLookingAtMe(player, 0.025, true, false, eyeY)`.
bool StareTriggerGoal::staredAtBy(const MobContext& mob, const PlayerView& player) {
    if (mob.distanceSquaredTo(player.x, player.y, player.z) > kStareRange * kStareRange) {
        return false;
    }
    const double eyeY = mob.mob.y + (mob.mob.height() * kEyeShare);
    double dx = mob.mob.x - player.x;
    double dy = eyeY - (player.y + kPlayerEye);
    double dz = mob.mob.z - player.z;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    if (distance < 1.0e-4) {
        return false;
    }
    dx /= distance;
    dy /= distance;
    dz /= distance;
    const double pitch = static_cast<double>(player.pitch) * std::numbers::pi / kDegrees;
    const double yaw = static_cast<double>(player.yaw) * std::numbers::pi / kDegrees;
    const double lookX = -std::sin(yaw) * std::cos(pitch);
    const double lookY = -std::sin(pitch);
    const double lookZ = std::cos(yaw) * std::cos(pitch);
    const double dot = (lookX * dx) + (lookY * dy) + (lookZ * dz);
    return dot > 1.0 - (kCone / distance);
}

bool StareTriggerGoal::canUse(MobContext& mob) {
    pending_.reset();
    if (mob.mob.targetId.has_value()) {
        return false;
    }
    double bestDistance = 0.0;
    for (const PlayerView& player : mob.players) {
        if (!player.targetable || !staredAtBy(mob, player)) {
            continue;
        }
        const double distance = mob.distanceSquaredTo(player.x, player.y, player.z);
        if (pending_.has_value() && distance >= bestDistance) {
            continue;
        }
        bestDistance = distance;
        pending_ = player.entityId;
    }
    return pending_.has_value();
}

void StareTriggerGoal::start(MobContext& /*mob*/) {
    // parity: five ticks of being stared at before it comes for you.
    aggroTime_ = adjustedTickDelay(kAggroDelay);
}

bool StareTriggerGoal::canContinueToUse(MobContext& mob) {
    const PlayerView* player = pending_.has_value() ? mob.playerById(*pending_) : nullptr;
    if (player == nullptr || !staredAtBy(mob, *player)) {
        return false;
    }
    if (aggroTime_ > 0) {
        aggroTime_--;
        return true;
    }
    mob.mob.targetId = player->entityId;
    return false;
}

void StareTriggerGoal::stop(MobContext& /*mob*/) { pending_.reset(); }

void TeleportAwayGoal::tick(MobContext& mob) {
    const world::WorldState& world = mob.world;
    const auto blockX = static_cast<std::int32_t>(std::floor(mob.mob.x));
    const auto blockY = static_cast<std::int32_t>(std::floor(mob.mob.y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(mob.mob.z));
    // parity: `isSensitiveToWater`: water hurts it, and it leaves.
    if (isWater(world, blockX, blockY, blockZ)) {
        mob.actions.hurtMob(mob.mob, mob.mob, kWaterDamage);
        teleportRandomly(mob);
        return;
    }
    // parity: the teleport a hit provokes, which is what makes one so hard to corner.
    if (mob.mob.hurtBy.has_value() && mob.random.nextInt(kTeleportOnHurtOdds) == 0) {
        teleportRandomly(mob);
        return;
    }
    // parity: `customServerAiStep`, the daylight teleport for an enderman with nothing to chase.
    if (!mob.mob.targetId.has_value() && !mob.night && mob.light > kDaylight && mob.random.nextInt(kDayOdds) == 0) {
        teleportRandomly(mob);
    }
}

void TeleportAwayGoal::teleportRandomly(MobContext& mob) {
    const double x = mob.mob.x + ((mob.random.nextDouble() - 0.5) * kRange);
    const double y =
        mob.mob.y + (static_cast<double>(mob.random.nextInt(static_cast<std::int32_t>(kRange))) - (kRange / 2));
    const double z = mob.mob.z + ((mob.random.nextDouble() - 0.5) * kRange);
    const std::optional<double> landing = groundBelow(mob.world, x, y, z);
    if (!landing.has_value()) {
        return;
    }
    mob.mob.x = x;
    mob.mob.y = *landing;
    mob.mob.z = z;
    mob.mob.velocityX = 0.0;
    mob.mob.velocityY = 0.0;
    mob.mob.velocityZ = 0.0;
    navigation::stop(mob.mob);
    mob.actions.teleported(mob.mob);
}

} // namespace basalt::server::runtime::entity::ai
