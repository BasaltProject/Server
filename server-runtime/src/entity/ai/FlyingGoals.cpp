#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/FlyingGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace basalt::server::runtime::entity::ai {
namespace {

constexpr float kAttackLookSpeed = 30.0F;

void nudge(ServerEntity& mob, double x, double y, double z, double step) {
    const double dx = x - mob.x;
    const double dy = y - mob.y;
    const double dz = z - mob.z;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    if (distance < 0.3) {
        return;
    }
    mob.velocityX += dx / distance * step;
    mob.velocityY += dy / distance * step;
    mob.velocityZ += dz / distance * step;
}

/// parity: `Level.isEmptyBlock`, which is `isAir`.
[[nodiscard]] bool isAir(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    if (!state.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
    return block != nullptr && (block->path() == "air" || block->path() == "cave_air" || block->path() == "void_air");
}

[[nodiscard]] bool conductsRedstone(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    return state.has_value() && registry::blockSupport::conductsRedstone(*state);
}

/// parity: `Entity.getEyePosition`'s height, with a player's standing 1.62.
[[nodiscard]] double eyeOf(const TargetView& target) {
    return target.entity != nullptr ? mobBreathing::eyeHeightOf(*target.entity) : static_cast<double>(1.62F);
}

void strike(MobContext& mob, const TargetView& target) {
    mob.actions.swing(mob.mob);
    const float damage = entityCombat::attackDamageOf(mob.mob);
    if (target.playerId.has_value()) {
        mob.actions.hurtPlayer(mob.mob, *target.playerId, damage);
    } else if (target.entity != nullptr) {
        mob.actions.hurtMob(mob.mob, *target.entity, damage);
    }
}

} // namespace

bool GhastFloatGoal::canUse(MobContext& mob) {
    if (!mob.mob.moving) {
        return true;
    }
    const double dx = mob.mob.wantedX - mob.mob.x;
    const double dy = mob.mob.wantedY - mob.mob.y;
    const double dz = mob.mob.wantedZ - mob.mob.z;
    const double distance = (dx * dx) + (dy * dy) + (dz * dz);
    return distance < 1.0 || distance > 3600.0;
}

void GhastFloatGoal::start(MobContext& mob) {
    // parity: `getSuitableFlyToPosition` with no distance to blocks.
    constexpr std::int32_t kAttempts = 64;
    constexpr float kRange = 16.0F;
    const auto roll = [&mob] {
        const double x = mob.mob.x + static_cast<double>(((mob.random.nextFloat() * 2.0F) - 1.0F) * kRange);
        const double y = mob.mob.y + static_cast<double>(((mob.random.nextFloat() * 2.0F) - 1.0F) * kRange);
        const double z = mob.mob.z + static_cast<double>(((mob.random.nextFloat() * 2.0F) - 1.0F) * kRange);
        return std::array<double, 3>{x, y, z};
    };
    std::optional<std::array<double, 3>> result;
    for (std::int32_t attempt = 0; attempt < kAttempts && !result.has_value(); ++attempt) {
        const std::array<double, 3> point = roll();
        if (!mob.mob.homePosition.has_value() || mob.mob.homeRadius <= 0) {
            result = point;
            break;
        }
        const double hx = point[0] - ((*mob.mob.homePosition)[0] + 0.5);
        const double hy = point[1] - ((*mob.mob.homePosition)[1] + 0.5);
        const double hz = point[2] - ((*mob.mob.homePosition)[2] + 0.5);
        const auto radius = static_cast<double>(mob.mob.homeRadius);
        if ((hx * hx) + (hy * hy) + (hz * hz) < radius * radius) {
            result = point;
        }
    }
    std::array<double, 3> point = result.has_value() ? *result : roll();
    if (!result.has_value()) {
        // parity: past the sixty-four tries it takes a free roll and keeps it below the terrain top.
        const std::int32_t bx = static_cast<std::int32_t>(std::floor(point[0]));
        const std::int32_t by = static_cast<std::int32_t>(std::floor(point[1]));
        const std::int32_t bz = static_cast<std::int32_t>(std::floor(point[2]));
        if (const server::world::Chunk* chunk = mob.world.chunkIfLoaded(bx >> 4, bz >> 4); chunk != nullptr) {
            const std::int32_t top = chunk->heightAt(bx & 15, bz & 15, server::world::HeightmapType::MotionBlocking);
            if (top < by && top > mob.minY) {
                point[1] = mob.mob.y - std::abs(mob.mob.y - point[1]);
            }
        }
    }
    moveControl::setWanted(mob.mob, point[0], point[1], point[2], 1.0);
}

void GhastLookGoal::tick(MobContext& mob) {
    // parity: `Ghast.faceMovementDirection`.
    constexpr float kDegrees = 180.0F / std::numbers::pi_v<float>;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        mob.mob.yaw = -static_cast<float>(std::atan2(mob.mob.velocityX, mob.mob.velocityZ)) * kDegrees;
    } else if (mob.distanceSquaredTo(target->x, target->y, target->z) < 4096.0) {
        mob.mob.yaw = -static_cast<float>(std::atan2(target->x - mob.mob.x, target->z - mob.mob.z)) * kDegrees;
    }
    mob.mob.headYaw = mob.mob.yaw;
}

bool FireballAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void FireballAttackGoal::stop(MobContext& mob) {
    chargeTime_ = 0;
    mob.actions.setCharging(mob.mob, false);
}

void FireballAttackGoal::tick(MobContext& mob) {
    /// parity: sixty-four blocks, twenty ticks of charge, forty of rest, and the tell at ten.
    constexpr double kRangeSquared = 4096.0;
    constexpr std::int32_t kChargeTicks = 20;
    constexpr std::int32_t kCooldownTicks = 40;
    constexpr std::int32_t kChargeTell = 10;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    if (mob.distanceSquaredTo(target->x, target->y, target->z) < kRangeSquared) {
        lookControl::lookAt(mob.mob, target->x, target->y, target->z, kAttackLookSpeed);
        chargeTime_++;
        if (chargeTime_ == kChargeTicks) {
            mob.actions.shootFireball(mob.mob, target->x, target->y + (target->height / 2.0), target->z, true);
            chargeTime_ = -kCooldownTicks;
        }
    } else if (chargeTime_ > 0) {
        chargeTime_--;
    }
    mob.actions.setCharging(mob.mob, chargeTime_ > kChargeTell);
}

bool BlazeAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void BlazeAttackGoal::stop(MobContext& mob) {
    attackStep_ = 0;
    mob.actions.setCharging(mob.mob, false);
}

void BlazeAttackGoal::tick(MobContext& mob) {
    constexpr double kMeleeSquared = 4.0;
    constexpr std::int32_t kMeleeInterval = 20;
    constexpr std::int32_t kFirstDelay = 60;
    constexpr std::int32_t kBurstDelay = 6;
    constexpr std::int32_t kRestDelay = 100;
    constexpr std::int32_t kBurst = 4;
    constexpr float kBlazeLook = 10.0F;

    attackTime_--;
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    const double distance = mob.distanceSquaredTo(target->x, target->y, target->z);
    const double follow = mob.mob.attributeWithModifiers("minecraft:follow_range");
    if (distance < kMeleeSquared) {
        if (attackTime_ <= 0) {
            attackTime_ = kMeleeInterval;
            strike(mob, *target);
        }
        moveControl::setWanted(mob.mob, target->x, target->y, target->z, 1.0);
        return;
    }
    if (distance < follow * follow) {
        if (attackTime_ <= 0) {
            attackStep_++;
            if (attackStep_ == 1) {
                attackTime_ = kFirstDelay;
                mob.actions.setCharging(mob.mob, true);
            } else if (attackStep_ <= kBurst) {
                attackTime_ = kBurstDelay;
            } else {
                attackTime_ = kRestDelay;
                attackStep_ = 0;
                mob.actions.setCharging(mob.mob, false);
            }
            if (attackStep_ > 1) {
                mob.actions.shootFireball(mob.mob, target->x, target->y + (target->height / 2.0), target->z, false);
            }
        }
        lookControl::lookAt(mob.mob, target->x, target->y + (target->height / 2.0), target->z, kBlazeLook, kBlazeLook);
        return;
    }
    // parity: out of range it closes in, as it does while it has seen the target in the last five.
    moveControl::setWanted(mob.mob, target->x, target->y, target->z, 1.0);
}

bool VexChargeGoal::canUse(MobContext& mob) {
    // parity: one in seven, not charging already, and farther than two blocks.
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value() || mob.mob.moving || mob.random.nextInt(reducedTickDelay(7)) != 0) {
        return false;
    }
    return mob.distanceSquaredTo(target->x, target->y, target->z) > 4.0;
}

bool VexChargeGoal::canContinueToUse(MobContext& mob) {
    return mob.mob.moving && mob.mob.charging && targetOf(mob).has_value();
}

void VexChargeGoal::start(MobContext& mob) {
    if (const std::optional<TargetView> target = targetOf(mob); target.has_value()) {
        moveControl::setWanted(mob.mob, target->x, target->y + eyeOf(*target), target->z, 1.0);
    }
    mob.actions.setCharging(mob.mob, true);
    mob.actions.mobSound(mob.mob, "entity.vex.charge");
}

void VexChargeGoal::stop(MobContext& mob) { mob.actions.setCharging(mob.mob, false); }

void VexChargeGoal::tick(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    const double half = mob.mob.width() / 2.0;
    const double targetHalf = target->width / 2.0;
    const bool touching = mob.mob.x - half < target->x + targetHalf && mob.mob.x + half > target->x - targetHalf &&
                          mob.mob.y < target->y + target->height && mob.mob.y + mob.mob.height() > target->y &&
                          mob.mob.z - half < target->z + targetHalf && mob.mob.z + half > target->z - targetHalf;
    if (touching) {
        strike(mob, *target);
        mob.actions.setCharging(mob.mob, false);
    } else if (mob.distanceSquaredTo(target->x, target->y, target->z) < 9.0) {
        moveControl::setWanted(mob.mob, target->x, target->y + eyeOf(*target), target->z, 1.0);
    }
}

bool VexRandomMoveGoal::canUse(MobContext& mob) {
    return !mob.mob.moving && mob.random.nextInt(reducedTickDelay(7)) == 0;
}

void VexRandomMoveGoal::tick(MobContext& mob) {
    const std::array<std::int32_t, 3> origin = mob.mob.flight.boundOrigin.value_or(std::array<std::int32_t, 3>{
        static_cast<std::int32_t>(std::floor(mob.mob.x)), static_cast<std::int32_t>(std::floor(mob.mob.y)),
        static_cast<std::int32_t>(std::floor(mob.mob.z))});
    for (std::int32_t attempt = 0; attempt < 3; ++attempt) {
        const std::int32_t x = origin[0] + mob.random.nextInt(15) - 7;
        const std::int32_t y = origin[1] + mob.random.nextInt(11) - 5;
        const std::int32_t z = origin[2] + mob.random.nextInt(15) - 7;
        if (!isAir(mob.world, x, y, z)) {
            continue;
        }
        moveControl::setWanted(mob.mob, x + 0.5, y + 0.5, z + 0.5, 0.25);
        if (!mob.mob.targetId.has_value()) {
            lookControl::lookAt(mob.mob, x + 0.5, y + 0.5, z + 0.5, 180.0F, 20.0F);
        }
        break;
    }
}

void BatFlightGoal::tick(MobContext& mob) {
    ServerEntity& bat = mob.mob;
    const std::int32_t bx = static_cast<std::int32_t>(std::floor(bat.x));
    const std::int32_t by = static_cast<std::int32_t>(std::floor(bat.y));
    const std::int32_t bz = static_cast<std::int32_t>(std::floor(bat.z));
    if (bat.flight.resting) {
        if (conductsRedstone(mob.world, bx, by + 1, bz)) {
            if (mob.random.nextInt(200) == 0) {
                bat.headYaw = static_cast<float>(mob.random.nextInt(360));
            }
            // parity: `BAT_RESTING_TARGETING`, anybody within four blocks wakes it.
            if (mob.nearestPlayer(4.0) != nullptr) {
                bat.flight.resting = false;
                mob.actions.poseChanged(bat);
                mob.actions.mobSound(bat, "entity.bat.takeoff");
            }
        } else {
            bat.flight.resting = false;
            mob.actions.poseChanged(bat);
            mob.actions.mobSound(bat, "entity.bat.takeoff");
        }
        return;
    }
    std::optional<std::array<std::int32_t, 3>>& target = bat.flight.batTarget;
    if (target.has_value() &&
        (!isAir(mob.world, (*target)[0], (*target)[1], (*target)[2]) || (*target)[1] <= mob.minY)) {
        target.reset();
    }
    const auto near = [&bat](const std::array<std::int32_t, 3>& at) {
        const double dx = at[0] + 0.5 - bat.x;
        const double dy = at[1] + 0.5 - bat.y;
        const double dz = at[2] + 0.5 - bat.z;
        return (dx * dx) + (dy * dy) + (dz * dz) < 4.0;
    };
    if (!target.has_value() || mob.random.nextInt(30) == 0 || near(*target)) {
        const double tx =
            bat.x + static_cast<double>(mob.random.nextInt(7)) - static_cast<double>(mob.random.nextInt(7));
        const double ty = bat.y + static_cast<double>(mob.random.nextInt(6)) - 2.0;
        const double tz =
            bat.z + static_cast<double>(mob.random.nextInt(7)) - static_cast<double>(mob.random.nextInt(7));
        target = std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(tx)),
                                             static_cast<std::int32_t>(std::floor(ty)),
                                             static_cast<std::int32_t>(std::floor(tz))};
    }
    const double dx = (*target)[0] + 0.5 - bat.x;
    const double dy = (*target)[1] + 0.1 - bat.y;
    const double dz = (*target)[2] + 0.5 - bat.z;
    const auto sign = [](double value) { return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0); };
    bat.velocityX += ((sign(dx) * 0.5) - bat.velocityX) * static_cast<double>(0.1F);
    bat.velocityY += ((sign(dy) * static_cast<double>(0.7F)) - bat.velocityY) * static_cast<double>(0.1F);
    bat.velocityZ += ((sign(dz) * 0.5) - bat.velocityZ) * static_cast<double>(0.1F);
    const auto yaw = static_cast<float>(std::atan2(bat.velocityZ, bat.velocityX) * (180.0 / std::numbers::pi)) - 90.0F;
    bat.yaw += moveControl::wrapDegrees(yaw - bat.yaw);
    if (mob.random.nextInt(100) == 0 && conductsRedstone(mob.world, bx, by + 1, bz)) {
        bat.flight.resting = true;
        mob.actions.poseChanged(bat);
    }
}

namespace {

/// parity: `Guardian`'s own eighty ticks of beam and the two points it adds.
constexpr std::int32_t kChargeTicks = 80;
constexpr float kBeamBonus = 2.0F;
constexpr double kKeepRange = 12.0;
constexpr double kSwimSpeed = 0.03;

} // namespace

bool BeamAttackGoal::canUse(MobContext& mob) { return targetOf(mob).has_value(); }

void BeamAttackGoal::start(MobContext& mob) {
    attackTime_ = -kChargeTicks;
    mob.actions.setBeamTarget(mob.mob, mob.mob.targetId);
}

void BeamAttackGoal::stop(MobContext& mob) {
    mob.actions.setBeamTarget(mob.mob, std::nullopt);
    attackTime_ = 0;
}

void BeamAttackGoal::tick(MobContext& mob) {
    const std::optional<TargetView> target = targetOf(mob);
    if (!target.has_value()) {
        return;
    }
    lookControl::lookAt(mob.mob, target->x, target->y, target->z, kAttackLookSpeed);
    attackTime_++;
    if (attackTime_ == 0) {
        mob.actions.setBeamTarget(
            mob.mob,
            target->playerId.has_value()
                ? target->playerId
                : (target->entity != nullptr ? std::optional<std::int32_t>{target->entity->entityId()} : std::nullopt));
    } else if (attackTime_ >= kChargeTicks) {
        const float damage = entityCombat::attackDamageOf(mob.mob) + kBeamBonus;
        if (target->playerId.has_value()) {
            mob.actions.hurtPlayer(mob.mob, *target->playerId, damage);
        } else if (target->entity != nullptr) {
            mob.actions.hurtMob(mob.mob, *target->entity, damage);
        }
        mob.mob.targetId.reset();
        attackTime_ = -kChargeTicks;
    }
    // parity: it keeps its distance while the beam charges.
    if (std::sqrt(mob.distanceSquaredTo(target->x, target->y, target->z)) > kKeepRange) {
        nudge(mob.mob, target->x, target->y, target->z, kSwimSpeed);
    }
}

} // namespace basalt::server::runtime::entity::ai
