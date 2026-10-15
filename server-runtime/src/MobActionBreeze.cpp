#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Breezes.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
namespace breezes = entity::breezes;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using breezes::BreezeState;
using breezes::Pose;

/// parity: `Sensor`'s sixteen-block reach for an attackable target.
constexpr double kTargetingRange = 16.0;

[[nodiscard]] float spreadFor(std::string_view difficulty) {
    const std::int32_t id = difficulty == "peaceful" ? 0 : difficulty == "easy" ? 1 : difficulty == "hard" ? 3 : 2;
    return static_cast<float>(5 - (id * 4));
}

} // namespace

float MobActionSystem::breezeTargetHeadYaw(const entity::ai::MobContext& context, std::int32_t targetId) {
    if (const entity::ai::PlayerView* player = context.playerById(targetId); player != nullptr) {
        return player->yaw;
    }
    const ServerEntity* mob = context.entityById(targetId);
    return mob == nullptr ? 0.0F : mob->headYaw;
}

void MobActionSystem::tickBreeze(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& breeze = context.mob;
    BreezeState& state = breezes::stateOf(breeze);
    const std::int64_t now = context.gameTime;
    const bool inWater = entity::ai::inWater(context.world, breeze);
    // parity: the core's `Swim(0.8)`.
    if (inWater && context.random.nextFloat() < 0.8F) {
        breeze.velocityY += 0.04;
    }
    // parity: `HURT_BY`, which lasts while the last damage is under two seconds old.
    if (breeze.hurtBy.has_value()) {
        state.hurtBy.setFor(*std::exchange(breeze.hurtBy, std::nullopt), now, 40);
    }
    // parity: `BreezeAttackEntitySensor` and `Breeze.canAttack`.
    const auto attackable = [&](const kit::Seen& seen) {
        if (seen.player != nullptr) {
            return seen.player->targetable;
        }
        return seen.mob != nullptr && seen.type == "iron_golem" && !seen.mob->dying && !seen.mob->removed;
    };
    const auto inSight = [&](const kit::Vec& at) {
        return breezes::lineOfSight(context.world, breeze, at) ||
               breezes::lineOfSight(context.world, breeze, {at[0], at[1] + 1.0, at[2]});
    };
    std::optional<std::int32_t> nearestAttackable;
    for (const kit::Seen& seen : kit::nearby(context, kTargetingRange, false)) {
        if (!attackable(seen)) {
            continue;
        }
        if (const std::optional<kit::Vec> at = kit::positionOf(context, seen.id, false);
            at.has_value() && inSight(*at)) {
            nearestAttackable = seen.id;
            break;
        }
    }
    // parity: `StopAttackingIfTargetInvalid`.
    // hundred ticks let go.
    std::optional<kit::Vec> targetAt;
    if (breeze.targetId.has_value()) {
        targetAt = kit::positionOf(context, *breeze.targetId, false);
        const entity::ai::PlayerView* player = context.playerById(*breeze.targetId);
        const ServerEntity* mob = context.entityById(*breeze.targetId);
        const bool alive =
            (player != nullptr && player->targetable) || (mob != nullptr && !mob->dying && !mob->removed);
        if (nearestAttackable == breeze.targetId) {
            state.targetSeen = now;
        }
        if (!targetAt.has_value() || !alive || now - state.targetSeen > breezes::kRememberTarget) {
            breeze.targetId.reset();
            targetAt.reset();
        }
    }
    const bool fight = breeze.targetId.has_value() && !state.motion.walk.has_value();
    if (!fight) {
        // parity: `StartAttacking` from the nearest attackable, then from whoever hurt it.
        if (!breeze.targetId.has_value() && nearestAttackable.has_value()) {
            breeze.targetId = nearestAttackable;
            state.targetSeen = now;
        }
        if (!breeze.targetId.has_value() && state.hurtBy.has(now)) {
            const std::int32_t by = *state.hurtBy.value;
            const ServerEntity* mob = context.entityById(by);
            if (context.playerById(by) != nullptr || (mob != nullptr && mob->type().path() == "iron_golem")) {
                breeze.targetId = by;
                state.targetSeen = now;
            }
        }
        // parity: `SlideToTargetSink(20, 40)`, the walk with its own pose and sound.
        if (!state.sliding && state.motion.walk.has_value()) {
            state.sliding = true;
            state.slideEnd = now + 20 + context.random.nextInt(21);
            playSound(world, breeze, "entity.breeze.slide", 1.0F, 1.0F);
            state.pose = Pose::Sliding;
        }
        // parity: the `RunOne` of doing nothing and a stroll at six tenths.
        if (state.idleTicks > 0) {
            --state.idleTicks;
        } else {
            for (const std::size_t choice : kit::shuffled(context.random, {1, 2})) {
                if (choice == 0) {
                    state.idleTicks = 20 + context.random.nextInt(81);
                    break;
                }
                if (!state.motion.walk.has_value()) {
                    if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                        kit::walkTo(state.motion, *spot, 0.6F, 0);
                    }
                    break;
                }
            }
        }
    } else {
        breezeFights(world, context, state, *targetAt, inWater);
    }
    if (state.shooting) {
        breezeShoots(world, context, state);
    }
    if (state.jumping) {
        breezeJumps(world, context, state);
    }
    kit::moveToTarget(context, state.motion);
    // parity: `SlideToTargetSink.stop`.
    if (state.sliding && (!state.motion.walk.has_value() || now > state.slideEnd)) {
        state.sliding = false;
        state.motion.walk.reset();
        entity::ai::navigation::stop(breeze);
        state.pose = Pose::Standing;
        if (breeze.targetId.has_value()) {
            state.shoot.setFor(true, now, 60);
        }
    }
    kit::lookAtTarget(context, state.look);
}

void MobActionSystem::breezeFights(world::LoadedWorld& world, entity::ai::MobContext& context, BreezeState& state,
                                   const kit::Vec& target, bool inWater) {
    ServerEntity& breeze = context.mob;
    const std::int64_t now = context.gameTime;
    const bool walkFree = !state.motion.walk.has_value();
    // parity: `Shoot`, charging, firing and recovering, within sixteen blocks only.
    if (!state.shooting && !state.shootCooldown.has(now) && !state.shootCharging.has(now) &&
        !state.shootRecovering.has(now) && state.shoot.has(now) && walkFree && !state.jumpTarget.has_value() &&
        state.pose == Pose::Standing) {
        const double dx = target[0] - breeze.x;
        const double dy = target[1] - breeze.y;
        const double dz = target[2] - breeze.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) < 256.0) {
            state.shooting = true;
            state.shootEnd = now + breezes::kShootDuration;
            state.pose = Pose::Shooting;
            state.shootCharging.setFor(true, now, breezes::kShootCharging);
            playSound(world, breeze, "entity.breeze.inhale", 1.0F, 1.0F);
        } else {
            state.shoot.erase();
        }
    }
    // parity: `LongJump`, off the ground to a spot behind the target.
    if (!state.jumping && !state.jumpCooldown.has(now) && !state.shoot.has(now) && walkFree &&
        breezeCanJump(context, state, target)) {
        breezeStartsJump(world, context, state);
    }
    if (!breeze.targetId.has_value()) {
        return;
    }
    // parity: `ShootWhenStuck`: riding, in water or floating, it shoots instead.
    const bool levitating = breeze.effects.contains(wire::common::Identifier::vanilla("levitation"));
    if (!state.jumpInhaling.has(now) && !state.jumpTarget.has_value() && walkFree && !state.shoot.has(now) &&
        (breeze.vehicleId.has_value() || inWater || levitating)) {
        state.shoot.setFor(true, now, 60);
    }
    // parity: `Slide`.
    if (!state.jumpCooldown.has(now) && walkFree && !state.shoot.has(now) && breeze.onGround && !inWater &&
        state.pose == Pose::Standing) {
        const kit::Vec centre{mth::floor(breeze.x) + 0.5, mth::floor(breeze.y) + 0.5, mth::floor(breeze.z) + 0.5};
        const double hx = target[0] - centre[0];
        const double hz = target[2] - centre[2];
        const bool inner = (hx * hx) + (hz * hz) < 16.0 && std::abs(target[1] - centre[1]) < 10.0;
        std::optional<kit::Vec> position;
        if (inner) {
            const std::optional<kit::Vec> away =
                entity::ai::randomPos::defaultPosAway(context.world, breeze, context.random, 5, 5, target);
            const auto distanceFromTarget = [&](const kit::Vec& at) {
                return ((at[0] - target[0]) * (at[0] - target[0])) + ((at[1] - target[1]) * (at[1] - target[1])) +
                       ((at[2] - target[2]) * (at[2] - target[2]));
            };
            if (away.has_value() && breezes::lineOfSight(context.world, breeze, *away) &&
                distanceFromTarget(*away) > distanceFromTarget({breeze.x, breeze.y, breeze.z})) {
                position = away;
            }
        }
        if (!position.has_value()) {
            if (context.random.nextBoolean()) {
                position = breezes::pointBehind(target, breezeTargetHeadYaw(context, *breeze.targetId), context.random);
            } else {
                const kit::Vec to{target[0] - breeze.x, target[1] - breeze.y, target[2] - breeze.z};
                const double length = std::sqrt((to[0] * to[0]) + (to[1] * to[1]) + (to[2] * to[2]));
                const double distance = length - (8.0 + (context.random.nextDouble() * (4.0 - 8.0)));
                const double scale = length < 1.0E-4 ? 0.0 : distance / length;
                position = kit::Vec{breeze.x + (to[0] * scale), breeze.y + (to[1] * scale), breeze.z + (to[2] * scale)};
            }
        }
        kit::walkTo(state.motion,
                    {mth::floor((*position)[0]) + 0.5, static_cast<double>(mth::floor((*position)[1])),
                     mth::floor((*position)[2]) + 0.5},
                    0.6F, 1);
    }
}

void MobActionSystem::breezeShoots(world::LoadedWorld& world, entity::ai::MobContext& context, BreezeState& state) {
    ServerEntity& breeze = context.mob;
    const std::int64_t now = context.gameTime;
    if (now > state.shootEnd || !breeze.targetId.has_value() || !state.shoot.has(now)) {
        // parity: `Shoot.stop`: standing again, ten ticks' rest, and the shot forgotten.
        state.shooting = false;
        if (state.pose == Pose::Shooting) {
            state.pose = Pose::Standing;
        }
        state.shootCooldown.setFor(true, now, breezes::kShootCooldown);
        state.shoot.erase();
        return;
    }
    const std::optional<kit::Vec> target = kit::positionOf(context, *breeze.targetId, false);
    if (!target.has_value()) {
        return;
    }
    breezes::lookAt(breeze, *target);
    if (state.shootCharging.has(now) || state.shootRecovering.has(now)) {
        return;
    }
    state.shootRecovering.setFor(true, now, breezes::kShootRecovering);
    const entity::projectiles::Kind* kind =
        entity::projectiles::kindOf(wire::common::Identifier::vanilla("breeze_wind_charge"));
    const server::registry::EntityTypeEntry* entry =
        kind == nullptr ? nullptr : server::registry::entityTypes::of(kind->type);
    if (entry == nullptr) {
        return;
    }
    // parity: `Shoot.tick`: at the target's lower third, or higher on one that is riding.
    double targetHeight = 1.8;
    bool riding = false;
    if (const entity::ai::PlayerView* player = context.playerById(*breeze.targetId); player != nullptr) {
        targetHeight = player->height;
        riding = player->vehicleId.has_value();
    } else if (const ServerEntity* mob = context.entityById(*breeze.targetId); mob != nullptr) {
        targetHeight = mob->height();
        riding = mob->vehicleId.has_value();
    }
    const double firingY =
        breeze.y + (static_cast<double>(static_cast<float>(breeze.height()) / 2.0F)) + static_cast<double>(0.3F);
    const double aimY = (*target)[1] + (targetHeight * (riding ? 0.8 : 0.3));
    random::RandomSource random = freshRandom();
    ServerEntity& charge =
        entity::projectiles::spawn(world.state(), *kind, players_->allocateEntityId(), entry->protocolId, breeze.x,
                                   firingY, breeze.z, (*target)[0] - breeze.x, aimY - firingY, (*target)[2] - breeze.z,
                                   breezes::kWindChargeSpeed, spreadFor(world.difficulty()), random);
    charge.ownerId = breeze.entityId();
    playSound(world, breeze, "entity.breeze.shoot", 1.5F, 1.0F);
}

} // namespace basalt::server::runtime
