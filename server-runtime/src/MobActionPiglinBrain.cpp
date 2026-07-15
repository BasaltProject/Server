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

void MobActionSystem::piglinActivity(world::LoadedWorld& world, entity::ai::MobContext& context, PiglinState& state) {
    ServerEntity& piglin = context.mob;
    const Sense& sense = tSense;
    const std::int64_t now = context.gameTime;
    switch (state.activity) {
    case Activity::AdmireItem: {
        // parity: `GoToWantedItem(1.0, interrupt, 9)` while the off hand holds nothing it loves.
        std::optional<world::DroppedItem> wanted;
        if (sense.wantedItem.has_value()) {
            for (const world::DroppedItem& item : world.state().nearbyItems(piglin.x, piglin.y, piglin.z, 33.0)) {
                if (item.entityId == *sense.wantedItem) {
                    wanted = item;
                }
            }
        }
        const std::optional<server::world::loot::ItemStack> offhand =
            entity::mobEquipment::get(piglin, entity::mobEquipment::Slot::OffHand);
        const bool lovesOffhand = offhand.has_value() && piglinLoves(state, offhand->item);
        if (wanted.has_value() && !lovesOffhand &&
            context.distanceSquaredTo(wanted->x, wanted->y, wanted->z) <
                piglins::kMaxDistanceToWalkToItem * piglins::kMaxDistanceToWalkToItem) {
            kit::walkTo(state.motion, {wanted->x, wanted->y, wanted->z}, 1.0F, 0);
            state.look = kit::Look{.at = kit::Vec{wanted->x, wanted->y, wanted->z}};
        }
        if (!offhand.has_value()) {
            // parity: `StopAdmiringIfItemTooFarAway(9)`.
            if (!wanted.has_value() || context.distanceSquaredTo(wanted->x, wanted->y, wanted->z) >=
                                           piglins::kMaxDistanceToWalkToItem * piglins::kMaxDistanceToWalkToItem) {
                state.admiring.erase();
            } else if (!state.timeTryingToReachItem.has_value()) {
                // parity: `StopAdmiringIfTiredOfTryingToReachItem(200, 200)`.
                state.timeTryingToReachItem = 0;
            } else if (*state.timeTryingToReachItem > piglins::kMaxTimeToWalkToItem) {
                state.admiring.erase();
                state.timeTryingToReachItem.reset();
                state.walkToItemDisabled.setFor(true, now, piglins::kDisableWalkToItem);
            } else {
                ++*state.timeTryingToReachItem;
            }
        }
        break;
    }
    case Activity::Fight:
        piglinFight(world, context, state);
        break;
    case Activity::Avoid: {
        // parity: `SetWalkTargetAwayFrom.entity(AVOID_TARGET, 1.0, 12, true)`.
        if (const std::optional<kit::Vec> from = kit::positionOf(context, *state.avoidTarget.value, false);
            from.has_value() && context.distanceSquaredTo((*from)[0], (*from)[1], (*from)[2]) < 144.0) {
            if (const std::optional<kit::Vec> flee =
                    entity::ai::randomPos::landPosAway(context.world, piglin, context.random, 16, 7, *from);
                flee.has_value()) {
                kit::walkTo(state.motion, *flee, 1.0F, 0);
            }
        }
        piglinIdle(world, context, state, true);
        // parity: `wantsToStopFleeing`.
        const ServerEntity* avoided = context.entityById(*state.avoidTarget.value);
        if (avoided != nullptr) {
            const std::string_view type = avoided->type().path();
            if (type == "hoglin") {
                // parity: `piglinsOutnumberHoglins`, the visible counts plus itself.
                if (sense.visibleAdultPiglins + 1 >= sense.adultHoglins) {
                    state.avoidTarget.erase();
                }
            } else if (isZombified(type) && sense.zombified != avoided->entityId()) {
                state.avoidTarget.erase();
            }
        } else {
            state.avoidTarget.erase();
        }
        break;
    }
    case Activity::Celebrate: {
        if (sense.repellent.has_value()) {
            const kit::Vec from{(*sense.repellent)[0] + 0.5, static_cast<double>((*sense.repellent)[1]),
                                (*sense.repellent)[2] + 0.5};
            if (const std::optional<kit::Vec> flee =
                    entity::ai::randomPos::landPosAway(context.world, piglin, context.random, 16, 7, from);
                flee.has_value() && !state.motion.walk.has_value()) {
                kit::walkTo(state.motion, *flee, 1.0F, 0);
            }
        }
        if (!piglin.targetId.has_value() && !isBaby(piglin)) {
            piglin.targetId = piglinAttackTarget(world, context, state);
        }
        // parity: `GoToTargetLocation`, two blocks and full speed, or four and dancing slow.
        const piglins::Pos location = *state.celebrateLocation.value;
        const std::int32_t close = state.dancing ? 4 : 2;
        const double dx = location[0] - mth::floor(piglin.x);
        const double dy = location[1] - mth::floor(piglin.y);
        const double dz = location[2] - mth::floor(piglin.z);
        if (!piglin.targetId.has_value() && !state.motion.walk.has_value() &&
            (dx * dx) + (dy * dy) + (dz * dz) >= close * close) {
            const kit::Vec near{location[0] + (context.random.nextInt(3) - 1) + 0.5, static_cast<double>(location[1]),
                                location[2] + (context.random.nextInt(3) - 1) + 0.5};
            kit::walkTo(state.motion, near, state.dancing ? 0.6F : 1.0F, close);
        }
        if (state.idleMoveTicks > 0) {
            --state.idleMoveTicks;
        } else {
            for (const std::size_t choice : kit::shuffled(context.random, {1, 1, 1})) {
                if (choice == 1 && !state.motion.walk.has_value()) {
                    if (const std::optional<kit::Vec> spot = kit::strollSpot(context, 2, 1); spot.has_value()) {
                        kit::walkTo(state.motion, *spot, 0.6F, 0);
                        break;
                    }
                    continue;
                }
                if (choice == 2) {
                    state.idleMoveTicks = 10 + context.random.nextInt(11);
                    break;
                }
                if (choice == 0) {
                    for (const kit::Seen& seen : kit::nearby(context, 8.0)) {
                        if (seen.type == "piglin") {
                            state.look = kit::Look{.entity = seen.id};
                            break;
                        }
                    }
                    break;
                }
            }
        }
        break;
    }
    case Activity::Ride: {
        // parity: `initRideHoglinActivity`, `Mount.create(0.8)` then `DismountOrSkipMounting.create(8, ...)`.
        ServerEntity* vehicle = piglin.vehicleId.has_value() ? context.entityById(*piglin.vehicleId) : nullptr;
        if (vehicle == nullptr && state.rideTarget.has(now)) {
            vehicle = context.entityById(*state.rideTarget.value);
        }
        const auto hurtRecently = [](const ServerEntity& mob) {
            // parity: `wasHurtRecently`, the `HURT_BY` memory of the last hundred ticks.
            return mob.lastHurtByMobTimestamp > 0 && mob.tickCount - mob.lastHurtByMobTimestamp <= 100;
        };
        const bool stopRiding = vehicle == nullptr || vehicle->removed || vehicle->dying || vehicle->health <= 0.0 ||
                                context.distanceSquaredTo(vehicle->x, vehicle->y, vehicle->z) >= 64.0 ||
                                !isBaby(*vehicle) || hurtRecently(piglin) || hurtRecently(*vehicle) ||
                                (vehicle->type().path() == "piglin" && !vehicle->vehicleId.has_value());
        if (stopRiding) {
            if (piglin.vehicleId.has_value()) {
                if (ServerEntity* ridden = context.entityById(*piglin.vehicleId); ridden != nullptr) {
                    std::erase(ridden->passengerIds, piglin.entityId());
                    sendPassengers(*ridden, watchersOf(world));
                }
                piglin.vehicleId.reset();
                piglin.boardingCooldown = 60;
            }
            state.rideTarget.erase();
            break;
        }
        if (!piglin.vehicleId.has_value()) {
            // parity: `Mount.create`, within one it climbs on, otherwise it walks over at 0.8.
            if (context.distanceSquaredTo(vehicle->x, vehicle->y, vehicle->z) < 1.0) {
                ServerEntity& ridden = isBaby(piglin) && vehicle->type().path() == "hoglin"
                                           ? *topPassengerOf(world, *vehicle, 3)
                                           : *vehicle;
                if (std::ranges::find(ridden.passengerIds, piglin.entityId()) == ridden.passengerIds.end()) {
                    ridden.passengerIds.push_back(piglin.entityId());
                    piglin.vehicleId = ridden.entityId();
                    sendPassengers(ridden, watchersOf(world));
                }
            } else {
                state.look = kit::Look{.entity = vehicle->entityId()};
                kit::walkTo(state.motion, {vehicle->x, vehicle->y, vehicle->z}, 0.8F, 1, vehicle->entityId());
            }
        }
        break;
    }
    case Activity::Idle:
        if (!state.brute && sense.playerHoldingLoved.has_value() && !state.look.entity.has_value()) {
            state.look = kit::Look{.entity = sense.playerHoldingLoved};
        }
        if (!piglin.targetId.has_value() && (state.brute || !isBaby(piglin))) {
            if (const std::optional<std::int32_t> target = piglinAttackTarget(world, context, state);
                target.has_value()) {
                piglin.targetId = target;
                state.motion.cantReachSince.reset();
                return;
            }
        }
        // parity: `StartHuntingHoglin`.
        if (!state.brute && !state.cannotHunt && !isBaby(piglin) && sense.huntableHoglin.has_value() &&
            !state.angryAt.has(now) && !state.huntedRecently.has(now)) {
            const bool othersHunted = std::ranges::any_of(sense.adultPiglins, [now](ServerEntity* other) {
                return other->piglin != nullptr && other->piglin->huntedRecently.has(now);
            });
            if (!othersHunted) {
                piglinSetAngerTarget(world, piglin, state, *sense.huntableHoglin);
                state.huntedRecently.setFor(true, now, sample(context.random, piglins::kTimeBetweenHunts));
                piglinBroadcastAnger(world, piglin, state, *sense.huntableHoglin);
                for (ServerEntity* other : sense.adultPiglins) {
                    if (other->piglin != nullptr) {
                        other->piglin->huntedRecently.setFor(true, now,
                                                             sample(context.random, piglins::kTimeBetweenHunts));
                    }
                }
            }
        }
        // parity: `avoidRepellent`, eight blocks from a soul fire.
        if (!state.brute && sense.repellent.has_value()) {
            const kit::Vec from{(*sense.repellent)[0] + 0.5, static_cast<double>((*sense.repellent)[1]),
                                (*sense.repellent)[2] + 0.5};
            if (context.distanceSquaredTo(from[0], from[1], from[2]) < 64.0) {
                if (const std::optional<kit::Vec> flee =
                        entity::ai::randomPos::landPosAway(context.world, piglin, context.random, 16, 7, from);
                    flee.has_value() && !state.motion.walk.has_value()) {
                    kit::walkTo(state.motion, *flee, 1.0F, 0);
                }
            }
        }
        piglinIdle(world, context, state, true);
        break;
    }
}

void MobActionSystem::piglinIdle(world::LoadedWorld& world, entity::ai::MobContext& context, PiglinState& state,
                                 bool moving) {
    static_cast<void>(world);
    ServerEntity& piglin = context.mob;
    const Sense& sense = tSense;
    // parity: `createIdleLookBehaviors`.
    if (state.idleLookTicks > 0) {
        --state.idleLookTicks;
    } else if (!state.look.entity.has_value() && !state.look.at.has_value()) {
        const std::vector<kit::Seen> seen = kit::nearby(context, 8.0);
        const std::vector<std::int32_t> weights =
            state.brute ? std::vector<std::int32_t>{1, 1, 1, 1, 1} : std::vector<std::int32_t>{1, 1, 1, 1};
        for (const std::size_t choice : kit::shuffled(context.random, weights)) {
            const std::size_t last = weights.size() - 1;
            if (choice == last) {
                state.idleLookTicks = 30 + context.random.nextInt(31);
                break;
            }
            const std::string_view wanted = choice == 0                    ? "player"
                                            : choice == 1                  ? "piglin"
                                            : (state.brute && choice == 2) ? "piglin_brute"
                                                                           : "";
            const auto found = std::ranges::find_if(
                seen, [wanted](const kit::Seen& one) { return wanted.empty() || one.type == wanted; });
            if (found != seen.end()) {
                state.look = kit::Look{.entity = found->id};
                break;
            }
        }
    }
    if (!moving) {
        return;
    }
    // parity: `createIdleMovementBehaviors`.
    if (state.idleMoveTicks > 0) {
        --state.idleMoveTicks;
        return;
    }
    const std::vector<std::int32_t> weights =
        state.brute ? std::vector<std::int32_t>{2, 2, 2, 2, 2, 1} : std::vector<std::int32_t>{2, 2, 2, 1};
    for (const std::size_t choice : kit::shuffled(context.random, weights)) {
        if (choice == weights.size() - 1) {
            state.idleMoveTicks = 30 + context.random.nextInt(31);
            return;
        }
        if (state.motion.walk.has_value()) {
            continue;
        }
        if (choice == 0) {
            if (const std::optional<kit::Vec> spot = kit::strollSpot(context); spot.has_value()) {
                kit::walkTo(state.motion, *spot, 0.6F, 0);
                return;
            }
            continue;
        }
        if (choice == 1 || (state.brute && choice == 2)) {
            const std::string_view wanted = choice == 1 ? "piglin" : "piglin_brute";
            for (const kit::Seen& seen : kit::nearby(context, 8.0)) {
                if (seen.type == wanted) {
                    kit::walkTo(state.motion, {seen.mob->x, seen.mob->y, seen.mob->z}, 0.6F, 2, seen.id);
                    state.look = kit::Look{.entity = seen.id};
                    return;
                }
            }
            continue;
        }
        if (!state.brute && choice == 2) {
            if (!sense.playerHoldingLoved.has_value() && state.look.entity.has_value()) {
                if (const std::optional<kit::Vec> at = kit::positionOf(context, *state.look.entity, false);
                    at.has_value()) {
                    kit::walkTo(state.motion, *at, 0.6F, 3);
                    return;
                }
            }
            continue;
        }
        // parity: `StrollToPoi(HOME, 0.6, 2, 100)` and `StrollAroundPoi(HOME, 0.6, 5)`.
        if (state.home.has_value()) {
            const kit::Vec home{(*state.home)[0] + 0.5, static_cast<double>((*state.home)[1]), (*state.home)[2] + 0.5};
            const double distance = context.distanceSquaredTo(home[0], home[1], home[2]);
            if (choice == 3 && distance > 4.0 && distance < 100.0 * 100.0) {
                kit::walkTo(state.motion, home, 0.6F, 2);
                return;
            }
            if (choice == 4 && distance < 25.0) {
                if (const std::optional<kit::Vec> spot = kit::strollSpot(context, 5, 7); spot.has_value()) {
                    kit::walkTo(state.motion, *spot, 0.6F, 0);
                    return;
                }
            }
        }
    }
    static_cast<void>(piglin);
}

void MobActionSystem::piglinFight(world::LoadedWorld& world, entity::ai::MobContext& context, PiglinState& state) {
    ServerEntity& piglin = context.mob;
    const std::int64_t now = context.gameTime;
    const std::optional<entity::ai::TargetView> target = entity::ai::targetOf(context);
    // parity: `StopAttackingIfTargetInvalid`.
    const std::optional<std::int32_t> nearest = piglinAttackTarget(world, context, state);
    if (!target.has_value() || nearest != piglin.targetId ||
        (state.motion.cantReachSince.has_value() && now - *state.motion.cantReachSince > 200)) {
        piglin.targetId.reset();
        state.crossbow = piglins::Crossbow::Uncharged;
        state.chargingCrossbow = false;
        state.spear = piglins::SpearStatus::Approach;
        state.spearChargePos.reset();
        state.spearFleeingPos.reset();
        state.spearFleeingTime = 0;
        return;
    }
    const bool crossbow = entity::mobEquipment::holds(piglin, "crossbow");
    // parity: `isWithinAttackRange`: eight blocks with a crossbow, the melee box without.
    const double dx = target->x - piglin.x;
    const double dy = target->y - piglin.y;
    const double dz = target->z - piglin.z;
    const double distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
    const bool inRange = crossbow ? distanceSquared < 64.0 : kit::withinMeleeRange(context, *target);
    state.look = kit::Look{.entity = piglin.targetId};
    const std::optional<server::world::loot::ItemStack> mainHand =
        entity::mobEquipment::get(piglin, entity::mobEquipment::Slot::MainHand);
    const bool spear = mainHand.has_value() && player::spear::of(mainHand->item.toString()) != nullptr;
    if (spear) {
        // parity: `SpearApproach`, `SpearAttack` and `SpearRetreat`, in that order.
        piglinSpearFight(world, context, state, *target, distanceSquared);
        return;
    }
    if (inRange) {
        state.motion.walk.reset();
    } else {
        kit::walkTo(state.motion, {target->x, target->y, target->z}, 1.0F, 0, piglin.targetId);
    }
    if (!crossbow) {
        // parity: `MeleeAttack.create(20)`, and `RememberIfHoglinWasKilled`.
        const bool struck = kit::meleeAttack(context, state.attackCoolingUntil, piglins::kMeleeCooldown);
        if (struck && target->entity != nullptr && target->entity->type().path() == "hoglin" &&
            (target->entity->dying || target->entity->health <= 0.0)) {
            state.huntedRecently.setFor(true, now, sample(context.random, piglins::kTimeBetweenHunts));
        }
        return;
    }
    // parity: `BackUpIfTooClose`, a sideways step off whenever the target comes within five.
    if (distanceSquared < 25.0) {
        entity::ai::moveControl::strafe(piglin, -0.75, 0.0, 0.75);
    }
    // parity: `CrossbowAttack`.
    if (!inRange) {
        state.crossbow = piglins::Crossbow::Uncharged;
        state.chargingCrossbow = false;
        return;
    }
    switch (state.crossbow) {
    case piglins::Crossbow::Uncharged:
        state.crossbow = piglins::Crossbow::Charging;
        state.crossbowTicks = 0;
        state.chargingCrossbow = true;
        break;
    case piglins::Crossbow::Charging:
        if (++state.crossbowTicks >= 25) {
            state.crossbow = piglins::Crossbow::Charged;
            state.crossbowDelay = 20 + context.random.nextInt(20);
            state.chargingCrossbow = false;
        }
        break;
    case piglins::Crossbow::Charged:
        if (--state.crossbowDelay == 0) {
            state.crossbow = piglins::Crossbow::ReadyToAttack;
        }
        break;
    case piglins::Crossbow::ReadyToAttack:
        // parity: `performCrossbowAttack(this, 1.6)`.
        shootArrow(piglin, target->x, target->y + (target->height / 3.0), target->z, 1.6F);
        state.crossbow = piglins::Crossbow::Uncharged;
        break;
    }
}

void MobActionSystem::piglinSpearFight(world::LoadedWorld& world, entity::ai::MobContext& context, PiglinState& state,
                                       const entity::ai::TargetView& target, double distanceSquared) {
    ServerEntity& piglin = context.mob;
    const std::optional<server::world::loot::ItemStack> mainHand =
        entity::mobEquipment::get(piglin, entity::mobEquipment::Slot::MainHand);
    const player::spear::Spear* spear = mainHand.has_value() ? player::spear::of(mainHand->item.toString()) : nullptr;
    if (spear == nullptr) {
        return;
    }
    // parity: the ridden distance bonus both reposition behaviours give a passenger.
    const std::int32_t mountDistance = piglin.vehicleId.has_value() ? 2 : 0;
    const kit::Vec targetPos{target.x, target.y, target.z};
    const auto awayFromTarget = [&](double maxDistance) {
        const int horizontal = static_cast<std::int32_t>(std::max(1.0, maxDistance));
        return entity::ai::randomPos::landPosAway(context.world, piglin, context.random, horizontal, 7, targetPos);
    };
    switch (state.spear) {
    case piglins::SpearStatus::Approach: {
        // parity: `SpearApproach`, walking up at full speed until ten blocks away.
        if (distanceSquared > 100.0) {
            kit::walkTo(state.motion, targetPos, 1.0F, 0, piglin.targetId);
            return;
        }
        // parity: `SpearApproach.stop`, which hands over to the charge.
        state.spear = piglins::SpearStatus::Charging;
        // parity: `SpearAttack.start`, the kinetic weapon's own use duration.
        state.spearEngageTime = spear->delayTicks + spear->damage.maxDurationTicks;
        state.spearChargePos.reset();
        return;
    }
    case piglins::SpearStatus::Charging: {
        // parity: `SpearAttack`, the charge runs its engage time then hands over to the retreat.
        --state.spearEngageTime;
        if (state.spearEngageTime <= 0) {
            state.spear = piglins::SpearStatus::Retreat;
            state.spearChargePos.reset();
            return;
        }
        if (state.spearChargePos.has_value()) {
            // parity: the charge position, walked off at repositioning speed until the path ends.
            kit::walkTo(state.motion,
                        kit::Vec{(*state.spearChargePos)[0] + 0.5, static_cast<double>((*state.spearChargePos)[1]),
                                 (*state.spearChargePos)[2] + 0.5},
                        1.0F, 0);
            if (!state.motion.walk.has_value() || entity::ai::navigation::isDone(piglin)) {
                state.spearChargePos.reset();
            }
            return;
        }
        kit::walkTo(state.motion, targetPos, 1.0F, 0, piglin.targetId);
        if (distanceSquared < 4.0 || entity::ai::navigation::isDone(piglin)) {
            const double distance = std::sqrt(distanceSquared);
            if (const std::optional<kit::Vec> away = awayFromTarget(7 + mountDistance - distance); away.has_value()) {
                state.spearChargePos =
                    kit::Block{mth::floor((*away)[0]), mth::floor((*away)[1]), mth::floor((*away)[2])};
            } else {
                state.spearChargePos.reset();
            }
        }
        return;
    }
    case piglins::SpearStatus::Retreat: {
        // parity: `SpearRetreat`, a position away kept for a hundred ticks at most.
        if (!state.spearFleeingPos.has_value()) {
            const double distance = std::sqrt(distanceSquared);
            const std::optional<kit::Vec> away = awayFromTarget(11 + mountDistance - distance);
            if (!away.has_value()) {
                state.spear = piglins::SpearStatus::Approach;
                state.spearFleeingTime = 0;
                return;
            }
            state.spearFleeingPos = kit::Block{mth::floor((*away)[0]), mth::floor((*away)[1]), mth::floor((*away)[2])};
            state.spearFleeingTime = 0;
        }
        if (state.spearFleeingTime >= 100 || !state.motion.walk.has_value()) {
            // parity: `SpearRetreat.stop`, which puts the approach back on.
            state.motion.walk.reset();
            state.spearFleeingTime = 0;
            state.spearFleeingPos.reset();
            state.spearChargePos.reset();
            state.spear = piglins::SpearStatus::Approach;
            return;
        }
        ++state.spearFleeingTime;
        kit::walkTo(state.motion,
                    kit::Vec{(*state.spearFleeingPos)[0] + 0.5, static_cast<double>((*state.spearFleeingPos)[1]),
                             (*state.spearFleeingPos)[2] + 0.5},
                    1.0F, 0);
        static_cast<void>(world);
        return;
    }
    }
}

} // namespace basalt::server::runtime
