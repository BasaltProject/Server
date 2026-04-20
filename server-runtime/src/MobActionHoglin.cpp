#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Hoglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cmath>
#include <memory>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
namespace hoglins = entity::hoglins;
namespace kit = entity::ai::brainKit;
using hoglins::Activity;
using hoglins::HoglinState;

[[nodiscard]] bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

struct HoglinSense {
    std::optional<kit::Block> repellent;
    std::optional<std::int32_t> adultPiglin;
    std::int32_t adultPiglins = 0;
    std::vector<ServerEntity*> adultHoglins;
    std::optional<std::int32_t> attackablePlayer;
};

[[nodiscard]] std::int32_t sample(random::RandomSource& random, const std::array<std::int32_t, 2>& range) {
    return range[0] + random.nextInt(range[1] - range[0] + 1);
}

} // namespace

namespace entity::hoglins {

HoglinState& stateOf(ServerEntity& hoglin) {
    if (hoglin.hoglin == nullptr) {
        hoglin.hoglin = std::make_shared<HoglinState>();
    }
    return *hoglin.hoglin;
}

} // namespace entity::hoglins

void MobActionSystem::tickHoglin(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& hoglin = context.mob;
    HoglinState& state = hoglins::stateOf(hoglin);
    const std::int64_t now = context.gameTime;
    // parity: `HoglinSpecificSensor`.
    HoglinSense sense;
    sense.repellent = kit::nearestBlock(context, 8, 4, [this](std::int32_t stateId) {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
        return block != nullptr && blockTagged("minecraft:hoglin_repellents", *block).value_or(false);
    });
    for (const kit::Seen& seen : kit::nearby(context, hoglin.attributeWithModifiers("minecraft:follow_range"))) {
        if (seen.player != nullptr) {
            if (seen.player->targetable && !sense.attackablePlayer.has_value()) {
                sense.attackablePlayer = seen.id;
            }
            continue;
        }
        if ((seen.type == "piglin" || seen.type == "piglin_brute") && !isBaby(*seen.mob)) {
            if (!sense.adultPiglin.has_value()) {
                sense.adultPiglin = seen.id;
            }
            ++sense.adultPiglins;
        } else if (seen.type == "hoglin" && !isBaby(*seen.mob)) {
            sense.adultHoglins.push_back(seen.mob);
        }
    }
    if (state.attackAnimation > 0) {
        --state.attackAnimation;
    }
    if (hoglin.hurtBy.has_value()) {
        const std::int32_t attacker = *std::exchange(hoglin.hurtBy, std::nullopt);
        // parity: `HoglinAi.wasHurtBy`.
        state.pacified.erase();
        state.breedTarget.reset();
        if (isBaby(hoglin)) {
            state.attackTarget.erase();
            hoglin.targetId.reset();
            state.motion.walk.reset();
            state.avoidTarget.setFor(attacker, now, sample(context.random, hoglins::kRetreatDuration));
        } else {
            const ServerEntity* by = world.state().entity(attacker);
            const std::string_view type =
                by == nullptr ? std::string_view{"player"} : std::string_view{by->type().path()};
            if ((state.activity != Activity::Avoid || type != "piglin") && type != "hoglin") {
                hoglinSetAttackTarget(world, hoglin, attacker);
                for (ServerEntity* other : sense.adultHoglins) {
                    if (!hoglins::stateOf(*other).pacified.has(now)) {
                        hoglinSetAttackTarget(world, *other, attacker);
                    }
                }
            }
        }
    }
    // parity: the target lives in `ATTACK_TARGET` for two hundred ticks at most.
    if (!state.attackTarget.has(now)) {
        state.attackTarget.erase();
        hoglin.targetId.reset();
    }
    const auto piglinsOutnumber = [&] {
        return !isBaby(hoglin) && sense.adultPiglins > static_cast<std::int32_t>(sense.adultHoglins.size()) + 1;
    };
    const Activity before = state.activity;
    state.activity = hoglin.targetId.has_value()  ? Activity::Fight
                     : state.avoidTarget.has(now) ? Activity::Avoid
                                                  : Activity::Idle;
    if (before != state.activity) {
        // parity: `getSoundForActivity`.
        std::string_view sound = "entity.hoglin.ambient";
        if (state.activity == Activity::Avoid || hoglin.conversionClock > 0) {
            sound = "entity.hoglin.retreat";
        } else if (state.activity == Activity::Fight) {
            sound = "entity.hoglin.angry";
        } else if (sense.repellent.has_value()) {
            sound = "entity.hoglin.retreat";
        }
        makeSound(world, hoglin, sound, 1.0F);
    }
    const auto becomePassive = [&] {
        // parity: `BecomePassiveIfMemoryPresent(NEAREST_REPELLENT, 200)`.
        if (sense.repellent.has_value() && !state.pacified.has(now)) {
            state.pacified.setFor(true, now, hoglins::kPacifyTime);
            state.attackTarget.erase();
            hoglin.targetId.reset();
        }
    };
    switch (state.activity) {
    case Activity::Idle: {
        becomePassive();
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 0.6F, 2);
        if (sense.repellent.has_value()) {
            const kit::Vec from{(*sense.repellent)[0] + 0.5, static_cast<double>((*sense.repellent)[1]),
                                (*sense.repellent)[2] + 0.5};
            if (context.distanceSquaredTo(from[0], from[1], from[2]) < 64.0) {
                if (const std::optional<kit::Vec> flee =
                        entity::ai::randomPos::landPosAway(context.world, hoglin, context.random, 16, 7, from);
                    flee.has_value()) {
                    kit::walkTo(state.motion, *flee, 1.0F, 0);
                }
            }
        }
        if (!state.pacified.has(now) && !state.breedTarget.has_value() && sense.attackablePlayer.has_value()) {
            hoglinSetAttackTarget(world, hoglin, *sense.attackablePlayer);
            break;
        }
        if (!isBaby(hoglin) && sense.adultPiglin.has_value() && !state.motion.walk.has_value()) {
            if (const std::optional<kit::Vec> from = kit::positionOf(context, *sense.adultPiglin, false);
                from.has_value() && context.distanceSquaredTo((*from)[0], (*from)[1], (*from)[2]) < 64.0) {
                if (const std::optional<kit::Vec> flee =
                        entity::ai::randomPos::landPosAway(context.world, hoglin, context.random, 16, 7, *from);
                    flee.has_value()) {
                    kit::walkTo(state.motion, *flee, 0.4F, 0);
                }
            }
        }
        kit::lookSometimes(context, state.look, state.lookTicker, 8.0, 30, 60);
        // parity: `BabyFollowAdult(5..16, 0.6)`.
        static_cast<void>(kit::followAdult(context, state.motion, state.look, 5, 16, 0.6F));
        kit::idleMovement(context, state.motion, state.look, state.idleTicks, 0.4F);
        break;
    }
    case Activity::Fight: {
        becomePassive();
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 0.6F, 2);
        if (!hoglin.targetId.has_value()) {
            break;
        }
        const std::optional<entity::ai::TargetView> target = entity::ai::targetOf(context);
        // parity: `StopAttackingIfTargetInvalid` and the erase while breeding.
        if (!target.has_value() || state.breedTarget.has_value() ||
            (state.motion.cantReachSince.has_value() && now - *state.motion.cantReachSince > 200)) {
            state.attackTarget.erase();
            hoglin.targetId.reset();
            break;
        }
        kit::chaseTarget(context, state.motion, state.look, 1.0F);
        const std::int32_t interval = isBaby(hoglin) ? hoglins::kBabyAttackInterval : hoglins::kAttackInterval;
        if (kit::meleeAttack(context, state.attackCoolingUntil, interval)) {
            // parity: `Hoglin.doHurtTarget`: the swing's event, its grunt, and `onHitTarget`.
            state.attackAnimation = hoglins::kAttackAnimation;
            entityEvent(hoglin, 4);
            makeSound(world, hoglin, "entity.hoglin.attack", 1.0F);
            if (!isBaby(hoglin)) {
                const bool piglin = target->entity != nullptr && target->entity->type().path() == "piglin";
                if (piglin && piglinsOutnumber()) {
                    for (ServerEntity* other : sense.adultHoglins) {
                        HoglinState& theirs = hoglins::stateOf(*other);
                        theirs.attackTarget.erase();
                        other->targetId.reset();
                        theirs.motion.walk.reset();
                        theirs.avoidTarget.setFor(*hoglin.targetId, now,
                                                  sample(context.random, hoglins::kRetreatDuration));
                    }
                    const std::int32_t avoided = *hoglin.targetId;
                    state.attackTarget.erase();
                    hoglin.targetId.reset();
                    state.motion.walk.reset();
                    state.avoidTarget.setFor(avoided, now, sample(context.random, hoglins::kRetreatDuration));
                } else {
                    for (ServerEntity* other : sense.adultHoglins) {
                        if (!hoglins::stateOf(*other).pacified.has(now)) {
                            hoglinSetAttackTarget(world, *other, *hoglin.targetId);
                        }
                    }
                }
            }
        }
        break;
    }
    case Activity::Avoid: {
        // parity: `SetWalkTargetAwayFrom.entity(AVOID_TARGET, 1.3, 15, false)`.
        if (const std::optional<kit::Vec> from = kit::positionOf(context, *state.avoidTarget.value, false);
            from.has_value() && !state.motion.walk.has_value() &&
            context.distanceSquaredTo((*from)[0], (*from)[1], (*from)[2]) < 225.0) {
            if (const std::optional<kit::Vec> flee =
                    entity::ai::randomPos::landPosAway(context.world, hoglin, context.random, 16, 7, *from);
                flee.has_value()) {
                kit::walkTo(state.motion, *flee, 1.3F, 0);
            }
        }
        kit::idleMovement(context, state.motion, state.look, state.idleTicks, 0.4F);
        kit::lookSometimes(context, state.look, state.lookTicker, 8.0, 30, 60);
        if (!isBaby(hoglin) && !piglinsOutnumber()) {
            state.avoidTarget.erase();
        }
        break;
    }
    }
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
}

void MobActionSystem::hoglinSetAttackTarget(world::LoadedWorld& world, ServerEntity& hoglin, std::int32_t targetId) {
    HoglinState& state = hoglins::stateOf(hoglin);
    const std::int64_t now = world.gameTime();
    // parity: `setAttackTargetIfCloserThanCurrent` keeps the nearer of the two.
    if (hoglin.targetId.has_value() && *hoglin.targetId != targetId) {
        const ServerEntity* current = world.state().entity(*hoglin.targetId);
        const ServerEntity* offered = world.state().entity(targetId);
        if (current != nullptr && offered != nullptr) {
            const auto distance = [&hoglin](const ServerEntity& other) {
                return ((other.x - hoglin.x) * (other.x - hoglin.x)) + ((other.y - hoglin.y) * (other.y - hoglin.y)) +
                       ((other.z - hoglin.z) * (other.z - hoglin.z));
            };
            if (distance(*current) <= distance(*offered)) {
                targetId = *hoglin.targetId;
            }
        }
    }
    state.motion.cantReachSince.reset();
    state.breedTarget.reset();
    state.attackTarget.setFor(targetId, now, hoglins::kAttackDuration);
    hoglin.targetId = targetId;
}

} // namespace basalt::server::runtime
