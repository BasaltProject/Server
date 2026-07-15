#include "MobActionPiglinShared.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/DoorInteraction.hpp>
#include <basalt/server/runtime/entity/ai/Goals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSpear.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime {
using namespace mobActionPiglin;

void MobActionSystem::tickPiglin(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& piglin = context.mob;
    PiglinState& state = piglins::stateOf(piglin);
    const std::int64_t now = context.gameTime;
    state.lastTick = now;
    settle(state.huntedRecently, now);
    if (state.angryAt.until < -1) {
        state.angryAt.until = now + (-2 - state.angryAt.until);
    }
    settle(state.universalAnger, now);
    if (!state.memoriesSeeded) {
        state.memoriesSeeded = true;
        // parity: `PiglinAi.initMemories` and `PiglinBruteAi.initMemories`, from `finalizeSpawn`.
        if (state.brute) {
            state.home = piglins::Pos{mth::floor(piglin.x), mth::floor(piglin.y), mth::floor(piglin.z)};
        } else {
            state.huntedRecently.setFor(true, now, sample(context.random, piglins::kTimeBetweenHunts));
        }
    }
    piglinSense(world, context, state);
    if (piglin.hurtBy.has_value()) {
        const std::int32_t attacker = *std::exchange(piglin.hurtBy, std::nullopt);
        piglinWasHurtBy(world, context, state, attacker);
    }
    piglinCore(world, context, state);
    // parity: `updateActivity`.
    const Activity before = state.activity;
    if (!state.brute && state.admiring.has(now)) {
        state.activity = Activity::AdmireItem;
    } else if (piglin.targetId.has_value()) {
        state.activity = Activity::Fight;
    } else if (!state.brute && state.avoidTarget.has(now)) {
        state.activity = Activity::Avoid;
    } else if (!state.brute && state.celebrateLocation.has(now)) {
        state.activity = Activity::Celebrate;
    } else if (!state.brute && state.rideTarget.has(now)) {
        state.activity = Activity::Ride;
    } else {
        state.activity = Activity::Idle;
    }
    if (before != state.activity) {
        if (state.brute) {
            if (state.activity == Activity::Fight) {
                makeSound(world, piglin, "entity.piglin_brute.angry", 1.0F);
            }
        } else {
            // parity: `getSoundForActivity`.
            std::string_view sound = "entity.piglin.ambient";
            if (state.activity == Activity::Fight) {
                sound = "entity.piglin.angry";
            } else if (piglin.conversionClock > 0 ||
                       (state.activity == Activity::Avoid && state.avoidTarget.has(now))) {
                sound = "entity.piglin.retreat";
            } else if (state.activity == Activity::AdmireItem) {
                sound = "entity.piglin.admiring_item";
            } else if (state.activity == Activity::Celebrate) {
                sound = "entity.piglin.celebrate";
            } else if (tSense.playerHoldingLoved.has_value()) {
                sound = "entity.piglin.jealous";
            } else if (tSense.repellent.has_value()) {
                sound = "entity.piglin.retreat";
            }
            makeSound(world, piglin, sound, 1.0F);
        }
    }
    if (!state.celebrateLocation.has(now)) {
        state.dancing = false;
    }
    // parity: `PiglinAi.updateActivity`, a rider without `RIDE_TARGET` climbs down.
    if (!state.rideTarget.has(now) && isBaby(piglin) && piglin.vehicleId.has_value()) {
        if (ServerEntity* vehicle = context.entityById(*piglin.vehicleId);
            vehicle != nullptr && vehicle->type().path() == "hoglin" && isBaby(*vehicle)) {
            std::erase(vehicle->passengerIds, piglin.entityId());
            sendPassengers(*vehicle, watchersOf(world));
            piglin.vehicleId.reset();
            piglin.boardingCooldown = 60;
        }
    }
    piglinActivity(world, context, state);
    kit::moveToTarget(context, state.motion);
    kit::lookAtTarget(context, state.look);
    // parity: `PiglinBruteAi.maybePlayActivitySound`, one in eighty a tick while fighting.
    if (state.brute && context.random.nextFloat() < 0.0125F && state.activity == Activity::Fight) {
        makeSound(world, piglin, "entity.piglin_brute.angry", 1.0F);
    }
    if (!state.brute && !piglin.dying && world.gameRules().get(world::gameRules::kMobGriefing)) {
        piglinPicksUp(world, piglin, state);
    }
}

void MobActionSystem::piglinSense(world::LoadedWorld& world, entity::ai::MobContext& context, PiglinState& state) {
    ServerEntity& piglin = context.mob;
    Sense& sense = tSense;
    sense = Sense{};
    const double range = piglin.attributeWithModifiers("minecraft:follow_range");
    // parity: `NearestLivingEntitySensor` has no vision filter; `PiglinSpecificSensor` filters `NEAREST_VISIBLE_*`.
    const double half = piglin.width() / 2.0;
    for (const ServerEntity* other : context.entities) {
        if (other == &piglin || other->removed || other->dying || other->health <= 0.0) {
            continue;
        }
        const double otherHalf = other->width() / 2.0;
        if (other->x + otherHalf < piglin.x - half - range || other->x - otherHalf > piglin.x + half + range ||
            other->y + other->height() < piglin.y - range || other->y > piglin.y + piglin.height() + range ||
            other->z + otherHalf < piglin.z - half - range || other->z - otherHalf > piglin.z + half + range) {
            continue;
        }
        if ((other->type().path() == "piglin" || other->type().path() == "piglin_brute") && !isBaby(*other)) {
            sense.adultPiglins.push_back(const_cast<ServerEntity*>(other));
        }
    }
    for (const kit::Seen& seen : kit::nearby(context, range)) {
        if (seen.mob != nullptr) {
            const std::string_view type = seen.type;
            if (type == "hoglin") {
                if (!isBaby(*seen.mob)) {
                    ++sense.adultHoglins;
                    const bool huntable =
                        !(seen.mob->sourceNbt.has_value() && seen.mob->sourceNbt->getBoolean("CannotBeHunted"));
                    if (!sense.huntableHoglin.has_value() && huntable) {
                        sense.huntableHoglin = seen.id;
                    }
                } else if (!sense.babyHoglin.has_value()) {
                    sense.babyHoglin = seen.id;
                }
            } else if (type == "piglin_brute" || (type == "piglin" && !isBaby(*seen.mob))) {
                ++sense.visibleAdultPiglins;
            } else if (!sense.nemesis.has_value() && (type == "wither_skeleton" || type == "wither")) {
                sense.nemesis = seen.id;
            } else if (!sense.zombified.has_value() && isZombified(type)) {
                sense.zombified = seen.id;
            }
            continue;
        }
        const entity::ai::PlayerView& view = *seen.player;
        PlayerSession* player = nullptr;
        for (PlayerSession* candidate : watchersOf(world)) {
            if (candidate->entityId() == view.entityId) {
                player = candidate;
            }
        }
        if (player == nullptr || player->dead) {
            continue;
        }
        const bool attackable = view.targetable;
        if (attackable && !sense.attackablePlayer.has_value()) {
            sense.attackablePlayer = view.entityId;
        }
        bool gold = false;
        for (std::int32_t slot = player::PlayerInventory::kArmourStart;
             slot < player::PlayerInventory::kArmourStart + player::PlayerInventory::kArmourSlots; ++slot) {
            const std::optional<storage::StoredItemStack>& worn = player->inventory.at(slot);
            if (worn.has_value()) {
                if (const auto id = wire::common::Identifier::parse(worn->id);
                    id.has_value() && itemTagged("minecraft:piglin_safe_armor", *id)) {
                    gold = true;
                }
            }
        }
        if (!sense.playerNotWearingGold.has_value() && !gold && attackable) {
            sense.playerNotWearingGold = view.entityId;
        }
        if (!sense.playerHoldingLoved.has_value() && view.heldItem.has_value() &&
            itemTagged("minecraft:piglin_loved", *view.heldItem)) {
            sense.playerHoldingLoved = view.entityId;
        }
    }
    // parity: `findNearestRepellent`, eight across and four up by manhattan distance.
    const piglins::Pos own{mth::floor(piglin.x), mth::floor(piglin.y), mth::floor(piglin.z)};
    for (std::int32_t depth = 0; depth <= 20 && !sense.repellent.has_value(); ++depth) {
        for (std::int32_t dx = -std::min(8, depth); dx <= std::min(8, depth) && !sense.repellent.has_value(); ++dx) {
            const std::int32_t restX = depth - std::abs(dx);
            for (std::int32_t dy = -std::min(4, restX); dy <= std::min(4, restX) && !sense.repellent.has_value();
                 ++dy) {
                const std::int32_t dz = restX - std::abs(dy);
                if (dz > 8) {
                    continue;
                }
                for (const std::int32_t sign : {1, -1}) {
                    if (sign < 0 && dz == 0) {
                        break;
                    }
                    const piglins::Pos at{own[0] + dx, own[1] + dy, own[2] + (sign * dz)};
                    const std::optional<std::int32_t> stateId = world.state().blockState(at[0], at[1], at[2]);
                    const wire::common::Identifier* block =
                        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
                    if (block == nullptr || !blockTagged("minecraft:piglin_repellents", *block).value_or(false)) {
                        continue;
                    }
                    // parity: a soul campfire only repels while it burns.
                    if (block->path() == "soul_campfire") {
                        const auto properties = registry::blockStateDefinitions::propertiesOf(*stateId);
                        if (const auto lit = properties.find("lit"); lit == properties.end() || lit->second != "true") {
                            continue;
                        }
                    }
                    sense.repellent = at;
                    break;
                }
            }
        }
    }
    // parity: `NearestItemSensor`: the nearest item within thirty-two it wants.
    if (!state.brute) {
        double best = 32.0 * 32.0;
        for (const world::DroppedItem& item : world.state().nearbyItems(piglin.x, piglin.y, piglin.z, 32.0)) {
            const server::world::loot::ItemStack stack{
                .item = item.item, .count = item.count, .components = item.components};
            const double distance = context.distanceSquaredTo(item.x, item.y, item.z);
            if (distance < best && piglinWantsToPickUp(piglin, state, stack, context.gameTime)) {
                best = distance;
                sense.wantedItem = item.entityId;
            }
        }
    }
}

void MobActionSystem::piglinCore(world::LoadedWorld& world, entity::ai::MobContext& context, PiglinState& state) {
    ServerEntity& piglin = context.mob;
    const Sense& sense = tSense;
    const std::int64_t now = context.gameTime;
    // parity: `InteractWithDoor`.
    {
        std::vector<ServerEntity*> nearest;
        for (ServerEntity* other : context.entities) {
            if (other != &piglin && std::abs(other->x - piglin.x) <= 16.0 && std::abs(other->y - piglin.y) <= 16.0 &&
                std::abs(other->z - piglin.z) <= 16.0) {
                nearest.push_back(other);
            }
        }
        entity::villager::VillagerWorld& doors = villagersIn(world);
        entity::ai::doorInteraction::tick(world.state(), piglin, state.doors, nearest,
                                          [&](const entity::ai::doorInteraction::Block& pos, bool open) {
                                              doors.setDoorOpen(piglin, {pos[0], pos[1], pos[2]}, open);
                                          });
    }
    if (!state.brute) {
        // parity: `babyAvoidNemesis` and `avoidZombified`.
        if (isBaby(piglin) && sense.nemesis.has_value() && !state.avoidTarget.has(now)) {
            state.avoidTarget.setFor(*sense.nemesis, now, sample(context.random, piglins::kAvoidZombified));
        }
        const std::optional<kit::Vec> zombified =
            sense.zombified.has_value() ? kit::positionOf(context, *sense.zombified, false) : std::nullopt;
        const bool nearZombified = zombified.has_value() &&
                                   context.distanceSquaredTo((*zombified)[0], (*zombified)[1], (*zombified)[2]) < 36.0;
        if (nearZombified && !state.avoidTarget.has(now)) {
            state.avoidTarget.setFor(*sense.zombified, now, sample(context.random, piglins::kAvoidZombified));
        }
        // parity: `babySometimesRideBabyHoglin`, a `SetEntityLookTargetSometimes.Ticker` on the level's random.
        if (isBaby(piglin) && !state.rideTarget.has(now) && sense.babyHoglin.has_value()) {
            if (state.rideTicker == 0) {
                state.rideTicker = sample(context.random, piglins::kRideStartInterval);
            }
            if (--state.rideTicker <= 0) {
                state.rideTicker = sample(context.random, piglins::kRideStartInterval);
                state.rideTarget.setFor(*sense.babyHoglin, now, sample(context.random, piglins::kRideDuration));
            }
        }
        // parity: `StopHoldingItemIfNoLongerAdmiring`, which keeps a shield.
        if (!state.admiring.has(now)) {
            const std::optional<server::world::loot::ItemStack> offhand =
                entity::mobEquipment::get(piglin, entity::mobEquipment::Slot::OffHand);
            if (offhand.has_value() && !offhand->components.contains("minecraft:blocks_attacks") &&
                offhand->item.path() != "shield") {
                piglinStopHoldingOffhand(world, piglin, state, true);
            }
        }
        // parity: `StartAdmiringItemIfSeen(119)`.
        if (sense.wantedItem.has_value() && !state.admiring.has(now) && !state.admiringDisabled.has(now) &&
            !state.walkToItemDisabled.has(now)) {
            for (const world::DroppedItem& item : world.state().nearbyItems(piglin.x, piglin.y, piglin.z, 33.0)) {
                if (item.entityId == *sense.wantedItem && piglinLoves(state, item.item)) {
                    state.admiring.setFor(true, now, piglins::kAdmireDuration);
                }
            }
        }
        // parity: `StartCelebratingIfTargetDead(300, wantsToDance)`.
        if (piglin.targetId.has_value() && !state.celebrateLocation.has(now)) {
            const ServerEntity* target = context.entityById(*piglin.targetId);
            const PlayerSession* deadPlayer = nullptr;
            for (PlayerSession* player : watchersOf(world)) {
                if (player->entityId() == *piglin.targetId && player->dead) {
                    deadPlayer = player;
                }
            }
            const bool dead = deadPlayer != nullptr ||
                              (target != nullptr && (target->dying || target->removed || target->health <= 0.0));
            if (dead) {
                random::RandomSource dance = random::RandomSource::legacy(now);
                if (target != nullptr && target->type().path() == "hoglin" && dance.nextFloat() < 0.1F) {
                    state.dancing = true;
                }
                const double x = deadPlayer != nullptr ? deadPlayer->x : target->x;
                const double y = deadPlayer != nullptr ? deadPlayer->y : target->y;
                const double z = deadPlayer != nullptr ? deadPlayer->z : target->z;
                state.celebrateLocation.setFor({mth::floor(x), mth::floor(y), mth::floor(z)}, now,
                                               piglins::kCelebrationTime);
                if (deadPlayer == nullptr || world.gameRules().get(world::gameRules::kForgiveDeadPlayers)) {
                    piglin.targetId.reset();
                    state.angryAt.erase();
                }
            }
        }
    }
    // parity: `StopBeingAngryIfTargetDead`, a dead player only with `forgive_dead_players`.
    if (state.angryAt.has(now)) {
        for (const ServerEntity* other : context.entities) {
            if (other->uuid() == *state.angryAt.value && (other->dying || other->health <= 0.0)) {
                state.angryAt.erase();
                break;
            }
        }
    }
    if (state.angryAt.has(now) && world.gameRules().get(world::gameRules::kForgiveDeadPlayers)) {
        for (const PlayerSession* player : watchersOf(world)) {
            if (player->uuid() == *state.angryAt.value && player->dead) {
                state.angryAt.erase();
                break;
            }
        }
    }
}

void MobActionSystem::piglinRetaliate(world::LoadedWorld& world, ServerEntity& piglin, PiglinState& state,
                                      std::int32_t attackerId, bool byPlayer) {
    if (!byPlayer || !world.gameRules().get(world::gameRules::kUniversalAnger)) {
        piglinSetAngerTarget(world, piglin, state, attackerId);
        piglinBroadcastAnger(world, piglin, state, attackerId);
        return;
    }
    // parity: `setAngerTargetToNearestTargetablePlayerIfFound`.
    piglinSetAngerTarget(world, piglin, state, tSense.attackablePlayer.value_or(attackerId));
    for (ServerEntity* adult : tSense.adultPiglins) {
        if (const std::optional<std::int32_t> player = piglinNearestPlayer(world, *adult); player.has_value()) {
            piglinSetAngerTarget(world, *adult, piglins::stateOf(*adult), *player);
        }
    }
}

std::optional<std::int32_t> MobActionSystem::piglinNearestPlayer(world::LoadedWorld& world,
                                                                 const ServerEntity& piglin) {
    const double range = piglin.attributeWithModifiers("minecraft:follow_range");
    const double eyeY = piglin.y + entity::mobBreathing::eyeHeightOf(piglin);
    std::optional<std::int32_t> nearest;
    double best = range * range;
    for (PlayerSession* player : watchersOf(world)) {
        if (player->dead || player->gamemode == wire::protocol::play::Gamemode::Creative ||
            player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const double dx = player->x - piglin.x;
        const double dy = player->y - piglin.y;
        const double dz = player->z - piglin.z;
        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
        if (distance >= best ||
            entity::blockRay::cast(world.state(), piglin.x, eyeY, piglin.z, player->x, player->y + 1.62, player->z)
                .has_value()) {
            continue;
        }
        best = distance;
        nearest = player->entityId();
    }
    return nearest;
}

std::optional<std::int32_t> MobActionSystem::piglinAttackTarget(world::LoadedWorld& world,
                                                                const entity::ai::MobContext& context,
                                                                PiglinState& state) {
    const Sense& sense = tSense;
    const std::int64_t now = context.gameTime;
    if (!state.brute && sense.zombified.has_value()) {
        if (const std::optional<kit::Vec> at = kit::positionOf(context, *sense.zombified, false);
            at.has_value() && context.distanceSquaredTo((*at)[0], (*at)[1], (*at)[2]) < 36.0) {
            return std::nullopt;
        }
    }
    // parity: `ANGRY_AT` first, whoever it names, if it can still be fought.
    if (state.angryAt.has(now)) {
        for (const entity::ai::PlayerView& player : context.players) {
            if (player.uuid == state.angryAt.value->toString() && player.targetable) {
                return player.entityId;
            }
        }
        for (const ServerEntity* other : context.entities) {
            if (other->uuid() == *state.angryAt.value && !other->dying && !other->removed) {
                return other->entityId();
            }
        }
    }
    static_cast<void>(world);
    if (state.brute) {
        return sense.attackablePlayer.has_value() ? sense.attackablePlayer : sense.nemesis;
    }
    // parity: `UNIVERSAL_ANGER` turns it on the nearest player it can see.
    if (state.universalAnger.has(now) && sense.attackablePlayer.has_value()) {
        return sense.attackablePlayer;
    }
    if (sense.nemesis.has_value()) {
        return sense.nemesis;
    }
    return sense.playerNotWearingGold;
}

} // namespace basalt::server::runtime
