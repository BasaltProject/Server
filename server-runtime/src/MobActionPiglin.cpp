#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
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
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
namespace piglins = entity::piglins;
namespace kit = entity::ai::brainKit;
namespace mth = wire::common::mth;
using piglins::Activity;
using piglins::PiglinState;

/// What the piglin sensor saw on its last look.
struct Sense {
    std::optional<std::int32_t> nemesis;
    std::optional<std::int32_t> huntableHoglin;
    std::optional<std::int32_t> zombified;
    std::optional<std::int32_t> playerNotWearingGold;
    std::optional<std::int32_t> playerHoldingLoved;
    std::optional<std::int32_t> attackablePlayer;
    std::vector<ServerEntity*> adultPiglins;
    std::int32_t adultHoglins = 0;
    std::optional<piglins::Pos> repellent;
    std::optional<std::int32_t> wantedItem;
};

/// Per piglin, what its sensors keep between looks. Filled every tick; see `piglinSense`.
thread_local Sense tSense;

[[nodiscard]] std::int32_t sample(random::RandomSource& random, const std::array<std::int32_t, 2>& range) {
    return range[0] + random.nextInt(range[1] - range[0] + 1);
}

[[nodiscard]] bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

[[nodiscard]] bool isZombified(std::string_view path) { return path == "zombified_piglin" || path == "zoglin"; }

void settle(kit::Memory<bool>& memory, std::int64_t now) {
    if (memory.until < -1) {
        memory.until = now + (-2 - memory.until);
    }
}

} // namespace

void MobActionSystem::tickPiglin(world::LoadedWorld& world, entity::ai::MobContext& context) {
    ServerEntity& piglin = context.mob;
    PiglinState& state = piglins::stateOf(piglin);
    const std::int64_t now = context.gameTime;
    state.lastTick = now;
    settle(state.huntedRecently, now);
    if (state.angryAt.until < -1) {
        state.angryAt.until = now + (-2 - state.angryAt.until);
    }
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
    // parity: `updateActivity`, the first of admire, fight, avoid, celebrate and idle that applies.
    const Activity before = state.activity;
    if (!state.brute && state.admiring.has(now)) {
        state.activity = Activity::AdmireItem;
    } else if (piglin.targetId.has_value()) {
        state.activity = Activity::Fight;
    } else if (!state.brute && state.avoidTarget.has(now)) {
        state.activity = Activity::Avoid;
    } else if (!state.brute && state.celebrateLocation.has(now)) {
        state.activity = Activity::Celebrate;
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
                }
            } else if (type == "piglin_brute" || (type == "piglin" && !isBaby(*seen.mob))) {
                sense.adultPiglins.push_back(seen.mob);
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
        // parity: `babyAvoidNemesis` and `avoidZombified`, which only copy into an empty avoid target.
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
                if (item.entityId == *sense.wantedItem && itemTagged("minecraft:piglin_loved", item.item)) {
                    state.admiring.setFor(true, now, piglins::kAdmireDuration);
                }
            }
        }
        // parity: `StartCelebratingIfTargetDead(300, wantsToDance)`.
        if (piglin.targetId.has_value() && !state.celebrateLocation.has(now)) {
            const ServerEntity* target = context.entityById(*piglin.targetId);
            const bool dead = target != nullptr && (target->dying || target->removed || target->health <= 0.0);
            if (dead) {
                random::RandomSource dance = random::RandomSource::legacy(now);
                if (target->type().path() == "hoglin" && dance.nextFloat() < 0.1F) {
                    state.dancing = true;
                }
                state.celebrateLocation.setFor({mth::floor(target->x), mth::floor(target->y), mth::floor(target->z)},
                                               now, piglins::kCelebrationTime);
                piglin.targetId.reset();
                state.angryAt.erase();
            }
        }
    }
    // parity: `StopBeingAngryIfTargetDead`.
    if (state.angryAt.has(now)) {
        for (const ServerEntity* other : context.entities) {
            if (other->uuid() == *state.angryAt.value && (other->dying || other->health <= 0.0)) {
                state.angryAt.erase();
                break;
            }
        }
    }
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
    if (sense.nemesis.has_value()) {
        return sense.nemesis;
    }
    return sense.playerNotWearingGold;
}

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
        const bool lovesOffhand = offhand.has_value() && itemTagged("minecraft:piglin_loved", offhand->item);
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
                if (static_cast<std::int32_t>(sense.adultPiglins.size()) + 1 >= sense.adultHoglins) {
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
    case Activity::Ride:
    case Activity::Idle:
        // parity: the idle activity's look at a player holding gold, the attack, the hunt, the
        // repellent, and the look and move shuffles.
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
    // parity: `createIdleLookBehaviors`: a player, a piglin or anybody within eight, or thirty to
    // sixty ticks of nothing.
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
    // parity: `createIdleMovementBehaviors`: a stroll, a walk to a piglin, a walk to what it looks
    // at, or nothing, and for a brute the strolls round its home.
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
    // parity: `StopAttackingIfTargetInvalid`: gone, unfightable, no longer the nearest valid one, or
    // two hundred ticks out of reach.
    const std::optional<std::int32_t> nearest = piglinAttackTarget(world, context, state);
    if (!target.has_value() || nearest != piglin.targetId ||
        (state.motion.cantReachSince.has_value() && now - *state.motion.cantReachSince > 200)) {
        piglin.targetId.reset();
        state.crossbow = piglins::Crossbow::Uncharged;
        state.chargingCrossbow = false;
        return;
    }
    const bool crossbow = entity::mobEquipment::holds(piglin, "crossbow");
    // parity: `isWithinAttackRange`: eight blocks with a crossbow, the melee box without.
    const double dx = target->x - piglin.x;
    const double dy = target->y - piglin.y;
    const double dz = target->z - piglin.z;
    const bool inRange = crossbow ? (dx * dx) + (dy * dy) + (dz * dz) < 64.0 : kit::withinMeleeRange(context, *target);
    state.look = kit::Look{.entity = piglin.targetId};
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
    // parity: `CrossbowAttack`: charge for the crossbow's own time, wait twenty to forty ticks, shoot.
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

} // namespace basalt::server::runtime
