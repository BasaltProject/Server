#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Axolotls.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <memory>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
namespace axolotls = entity::axolotls;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using axolotls::Activity;
using axolotls::AxolotlState;

[[nodiscard]] bool isWaterAt(const world::WorldState& world, double x, double y, double z) {
    const std::optional<std::int32_t> state = world.blockState(mth::floor(x), mth::floor(y), mth::floor(z));
    return state.has_value() && world::fluids::of(*state) == world::Submersion::Water;
}

} // namespace

namespace entity::axolotls {

AxolotlState& stateOf(ServerEntity& axolotl) {
    if (axolotl.axolotl == nullptr) {
        axolotl.axolotl = std::make_shared<AxolotlState>();
    }
    return *axolotl.axolotl;
}

bool playingDead(const ServerEntity& axolotl) {
    return axolotl.axolotl != nullptr && axolotl.axolotl->playDeadTicks.has_value() &&
           *axolotl.axolotl->playDeadTicks > 0;
}

} // namespace entity::axolotls

void MobActionSystem::tickAxolotl(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& axolotl = context.mob;
    AxolotlState& state = axolotls::stateOf(axolotl);
    const std::int64_t now = context.gameTime;
    const bool inWater = isWaterAt(context.world, axolotl.x, axolotl.y, axolotl.z);
    axolotl.smoothSwimming = inWater;
    // parity: `Axolotl.hurtServer`.
    if (axolotl.hurtBy.has_value()) {
        const std::int32_t attacker = *std::exchange(axolotl.hurtBy, std::nullopt);
        state.hurtByEntity = attacker;
        const double damage = static_cast<double>(axolotl.lastHurt);
        const double before = axolotl.health + damage;
        const double max = axolotl.attributeWithModifiers("minecraft:max_health");
        if (context.random.nextInt(3) == 0 &&
            (static_cast<double>(context.random.nextInt(3)) < damage || before / max < 0.5) && damage < before &&
            inWater && !axolotls::playingDead(axolotl)) {
            state.playDeadTicks = axolotls::kPlayDeadTime;
        }
    }
    // parity: `ValidatePlayDead` and the temptation cooldown, the core's counters.
    if (state.playDeadTicks.has_value()) {
        if (*state.playDeadTicks <= 0) {
            state.playDeadTicks.reset();
            state.hurtByEntity.reset();
            state.activity = Activity::Idle;
        } else {
            --*state.playDeadTicks;
        }
    }
    // parity: `updateActivity`, which leaves play-dead alone once it is in it.
    const Activity before = state.activity;
    if (before != Activity::PlayDead) {
        state.activity = state.playDeadTicks.has_value() ? Activity::PlayDead
                         : axolotl.targetId.has_value()  ? Activity::Fight
                                                         : Activity::Idle;
        if (before == Activity::Fight && state.activity != Activity::Fight) {
            state.huntingCooldown.setFor(true, now, axolotls::kHuntingCooldown);
        }
    }
    const float chaseSpeed = inWater ? 0.6F : 0.15F;
    const float idleSpeed = inWater ? 0.5F : 0.15F;
    switch (state.activity) {
    case Activity::PlayDead:
        // parity: `PlayDead`: in water, with whoever hurt it known; regeneration for its trouble.
        if (state.playDeadRun == 0 && inWater && state.hurtByEntity.has_value()) {
            state.playDeadRun = 200;
            state.motion.walk.reset();
            state.look = {};
            villagersIn(world).addEffect(axolotl, "regeneration", 200, 0);
        }
        if (state.playDeadRun > 0 && (!inWater || !state.playDeadTicks.has_value() || --state.playDeadRun == 0)) {
            state.playDeadRun = 0;
        }
        // parity: breeding cuts it short.
        if (kit::makeLove(context, state.motion, state.look, 0.2F, 2).has_value()) {
            state.playDeadTicks.reset();
        }
        if (!state.playDeadTicks.has_value()) {
            state.activity = Activity::Idle;
        }
        break;
    case Activity::Fight: {
        const std::optional<entity::ai::TargetView> target = entity::ai::targetOf(context);
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 0.2F, 2);
        const bool dead =
            target.has_value() && target->entity != nullptr && (target->entity->dying || target->entity->health <= 0.0);
        if (!target.has_value() || dead || state.breedTarget.has_value() ||
            (state.motion.cantReachSince.has_value() && now - *state.motion.cantReachSince > 200)) {
            // parity: `onStopAttacking`: a kill a nearby player helped with earns them regeneration.
            if (dead && target->entity->lastBlame.causingId.has_value()) {
                for (PlayerSession* player : watchersOf(world)) {
                    if (player->entityId() == *target->entity->lastBlame.causingId &&
                        std::abs(player->x - axolotl.x) <= axolotls::kPlayerRegenRange + 0.375 &&
                        std::abs(player->y - axolotl.y) <= axolotls::kPlayerRegenRange + 0.42 &&
                        std::abs(player->z - axolotl.z) <= axolotls::kPlayerRegenRange + 0.375) {
                        axolotlSupports(*player, axolotl.entityId());
                    }
                }
            }
            axolotl.targetId.reset();
            break;
        }
        kit::chaseTarget(context, state.motion, state.look, chaseSpeed);
        if (kit::meleeAttack(context, state.attackCoolingUntil, 20)) {
            playSound(world, axolotl, "entity.axolotl.attack");
        }
        break;
    }
    case Activity::Idle: {
        kit::lookSometimes(context, state.look, state.lookTicker, 6.0, 30, 60, "player");
        state.breedTarget = kit::makeLove(context, state.motion, state.look, 0.2F, 2);
        if (!state.breedTarget.has_value()) {
            // parity: the `RunOne` of following food and following an adult, one or the other.
            const bool tempted =
                kit::tempt(context, state.motion, state.look, state.temptation, idleSpeed,
                           [](const wire::common::Identifier& item) {
                               return entity::breeding::isFood(wire::common::Identifier::vanilla("axolotl"), item);
                           });
            if (!tempted) {
                static_cast<void>(kit::followAdult(context, state.motion, state.look, 5, 16, inWater ? 0.6F : 0.15F));
            }
        }
        // parity: `StartAttacking`, the nearest thing in water it hunts or always fights.
        if (!state.breedTarget.has_value() && !axolotl.targetId.has_value()) {
            const auto inTag = [this](std::string_view tag, const wire::common::Identifier& type) {
                return options_.entityTags != nullptr &&
                       world::realEntityTypeTags::membersOf(*options_.entityTags,
                                                            wire::common::Identifier::vanilla(std::string{tag}))
                           .contains(type);
            };
            for (const kit::Seen& seen : kit::nearby(context, 8.0)) {
                if (seen.mob == nullptr || seen.distanceSquared > 64.0 ||
                    !isWaterAt(context.world, seen.mob->x, seen.mob->y, seen.mob->z)) {
                    continue;
                }
                const bool hostile = inTag("axolotl_always_hostiles", seen.mob->type());
                const bool hunt = !state.huntingCooldown.has(now) && inTag("axolotl_hunt_targets", seen.mob->type());
                if (hostile || hunt) {
                    axolotl.targetId = seen.id;
                    break;
                }
            }
        }
        // parity: `TryFindLiquid(6, 0.15)`, the nearest water with air over it.
        if (!inWater && !axolotl.targetId.has_value() && !state.motion.walk.has_value() &&
            now >= state.nextLiquidSearch) {
            state.nextLiquidSearch = now + 22;
            std::optional<kit::Block> found;
            const kit::Block own{mth::floor(axolotl.x), mth::floor(axolotl.y), mth::floor(axolotl.z)};
            found = kit::nearestBlock(context, 6, 6, [&](std::int32_t stateId) {
                return world::fluids::of(stateId) == world::Submersion::Water;
            });
            if (found.has_value() && ((*found)[0] != own[0] || (*found)[2] != own[2])) {
                kit::walkTo(state.motion, {(*found)[0] + 0.5, static_cast<double>((*found)[1]), (*found)[2] + 0.5},
                            0.15F, 0);
            }
        }
        // parity: the gate of a swim, a land stroll and a walk to what it looks at, tried in order.
        if (!state.motion.walk.has_value()) {
            for (const std::size_t choice : kit::shuffled(context.random, {2, 2, 3, 5, 5})) {
                if (choice == 0 && inWater) {
                    if (const std::optional<kit::Vec> spot = kit::swimSpot(context); spot.has_value()) {
                        kit::walkTo(state.motion, *spot, 0.5F, 0);
                    }
                    break;
                }
                if (choice == 1 && !inWater) {
                    if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                        kit::walkTo(state.motion, *spot, 0.15F, 0);
                    }
                    break;
                }
                if (choice == 2 && (state.look.entity.has_value() || state.look.at.has_value())) {
                    std::optional<kit::Vec> at = state.look.at;
                    if (state.look.entity.has_value()) {
                        at = kit::positionOf(context, *state.look.entity, false);
                    }
                    if (at.has_value() && isWaterAt(context.world, (*at)[0], (*at)[1], (*at)[2]) == inWater) {
                        kit::walkTo(state.motion, *at, idleSpeed, 3);
                    }
                    break;
                }
                if ((choice == 3 && inWater) || (choice == 4 && axolotl.onGround)) {
                    break;
                }
            }
        }
        break;
    }
    }
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
}

void MobActionSystem::axolotlSupports(PlayerSession& player, std::int32_t axolotlId) {
    // parity: `applySupportingEffects`.
    const wire::common::Identifier regeneration = wire::common::Identifier::vanilla("regeneration");
    const auto existing = player.effects.find(regeneration);
    if (existing == player.effects.end() || existing->second.remainingTicks <= 2399) {
        const std::int32_t previous = existing == player.effects.end() ? 0 : existing->second.remainingTicks;
        applyEffect(player.entityId(), "regeneration", std::min(axolotls::kRegenMax, axolotls::kRegenBase + previous),
                    0, axolotlId);
    }
    const wire::common::Identifier fatigue = wire::common::Identifier::vanilla("mining_fatigue");
    if (player.effects.erase(fatigue) > 0 && player.connection().sendEffectRemoved) {
        player.connection().sendEffectRemoved(fatigue);
    }
}

} // namespace basalt::server::runtime
