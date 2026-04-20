#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/ThrownItemTriggers.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using server::world::loot::ItemStack;
using wire::protocol::play::Gamemode;
namespace piglins = entity::piglins;
namespace equipment = entity::mobEquipment;
namespace mth = wire::common::mth;
using piglins::PiglinState;

[[nodiscard]] bool isBaby(const ServerEntity& mob) { return mob.age < 0; }

/// parity: `Mob.getEquipmentSlotForItem`.
/// components per item.
[[nodiscard]] equipment::Slot slotFor(const ItemStack& stack) {
    const std::string_view path = stack.item.path();
    if (path.ends_with("_helmet") || path == "carved_pumpkin" || path.ends_with("_skull") || path.ends_with("_head") ||
        path == "turtle_helmet") {
        return equipment::Slot::Head;
    }
    if (path.ends_with("_chestplate") || path == "elytra") {
        return equipment::Slot::Chest;
    }
    if (path.ends_with("_leggings")) {
        return equipment::Slot::Legs;
    }
    if (path.ends_with("_boots")) {
        return equipment::Slot::Feet;
    }
    if (path == "shield") {
        return equipment::Slot::OffHand;
    }
    return equipment::Slot::MainHand;
}

} // namespace

bool MobActionSystem::itemTagged(std::string_view tag, const wire::common::Identifier& item) const {
    return options_.itemTags != nullptr && world::realItemTags::contains(*options_.itemTags, tag, item);
}

void MobActionSystem::equipmentChanged(ServerEntity& mob, std::uint8_t slot) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return;
    }
    using wire::protocol::play::SetEquipmentPacket;
    const std::optional<ItemStack> held = equipment::get(mob, static_cast<equipment::Slot>(slot));
    const server::registry::Item* type =
        held.has_value() ? server::registry::items::registry().byName(held->item) : nullptr;
    SetEquipmentPacket::Slot entry{.type = static_cast<SetEquipmentPacket::SlotType>(slot)};
    if (type != nullptr) {
        entry.stack = wire::protocol::play::WireItemStack{
            .itemId = type->protocolId,
            .count = held->count,
            .components = item::itemComponentCodecs::toWire(
                held->components, [](const wire::common::Identifier&) { return std::optional<std::int32_t>{}; })};
    }
    for (PlayerSession* player : watchersOf(*world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEquipmentPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                SetEquipmentPacket{.entityId = mob.entityId(), .slots = {entry}}.write(buffer);
            });
        }
    }
}

void MobActionSystem::makeSound(world::LoadedWorld& world, const ServerEntity& mob, std::string_view sound,
                                float volume) {
    if (mob.sourceNbt.has_value() && mob.sourceNbt->getBoolean("Silent")) {
        return;
    }
    random::RandomSource voice = freshRandom();
    // parity: `LivingEntity.getVoicePitch`, higher for a baby.
    const float pitch = ((voice.nextFloat() - voice.nextFloat()) * 0.2F) + (isBaby(mob) ? 1.5F : 1.0F);
    playSound(world, mob, sound, volume, pitch);
}

void MobActionSystem::throwFromMob(world::LoadedWorld& world, const ServerEntity& mob, const ItemStack& stack,
                                   const std::array<double, 3>& target) {
    const double tx = target[0] - mob.x;
    const double ty = target[1] - mob.y;
    const double tz = target[2] - mob.z;
    const double length = std::sqrt((tx * tx) + (ty * ty) + (tz * tz));
    const double scale = length < 1.0E-4 ? 0.0 : static_cast<double>(0.3F) / length;
    world::DroppedItem dropped{.entityId = players_->allocateEntityId(),
                               .uuid = wire::common::Uuid::random(),
                               .item = stack.item,
                               .count = stack.count,
                               .x = mob.x,
                               .y = mob.y + entity::mobBreathing::eyeHeightOf(mob) - static_cast<double>(0.3F),
                               .z = mob.z,
                               .components = stack.components};
    dropped.velocityX = tx * scale;
    dropped.velocityY = ty * scale;
    dropped.velocityZ = tz * scale;
    dropped.pickupDelay = world::kSpawnedPickupDelay;
    // parity: `BehaviorUtils.throwItem` sets the thrower.
    dropped.thrower = mob.uuid();
    world.state().spawnItem(std::move(dropped));
}

bool MobActionSystem::piglinWantsToPickUp(const ServerEntity& piglin, const PiglinState& state, const ItemStack& stack,
                                          std::int64_t now) const {
    // parity: `Piglin.wantsToPickUp` over `PiglinAi.wantsToPickup`.
    if (isBaby(piglin) && itemTagged("minecraft:ignored_by_piglin_babies", stack.item)) {
        return false;
    }
    if (itemTagged("minecraft:piglin_repellents", stack.item)) {
        return false;
    }
    if (state.admiringDisabled.has(now) && piglin.targetId.has_value()) {
        return false;
    }
    const std::optional<ItemStack> offhand = equipment::get(piglin, equipment::Slot::OffHand);
    const bool lovesOffhand = offhand.has_value() && itemTagged("minecraft:piglin_loved", offhand->item);
    if (stack.item.path() == "gold_ingot") {
        return !lovesOffhand;
    }
    const bool space = piglins::canAddToInventory(state, stack);
    if (stack.item.path() == "gold_nugget") {
        return space;
    }
    if (itemTagged("minecraft:piglin_food", stack.item)) {
        return !state.ateRecently.has(now) && space;
    }
    if (!itemTagged("minecraft:piglin_loved", stack.item)) {
        // parity: `canReplaceCurrentItem`.
        const equipment::Slot slot = slotFor(stack);
        const std::optional<ItemStack> current = equipment::get(piglin, slot);
        const bool newWanted = !isBaby(piglin) && itemTagged("minecraft:piglin_preferred_weapons", stack.item);
        const bool currentWanted =
            current.has_value() &&
            (itemTagged("minecraft:piglin_loved", current->item) ||
             (!isBaby(piglin) && itemTagged("minecraft:piglin_preferred_weapons", current->item)));
        if (newWanted && !currentWanted) {
            return true;
        }
        return !current.has_value() && !currentWanted;
    }
    return !lovesOffhand && space;
}

bool MobActionSystem::piglinEquip(world::LoadedWorld& world, ServerEntity& piglin, const ItemStack& stack) {
    const equipment::Slot slot = slotFor(stack);
    const std::optional<ItemStack> current = equipment::get(piglin, slot);
    const bool newWanted = !isBaby(piglin) && itemTagged("minecraft:piglin_preferred_weapons", stack.item);
    const bool currentWanted =
        current.has_value() && (itemTagged("minecraft:piglin_loved", current->item) ||
                                (!isBaby(piglin) && itemTagged("minecraft:piglin_preferred_weapons", current->item)));
    if (current.has_value() && !(newWanted && !currentWanted)) {
        return false;
    }
    if (current.has_value()) {
        throwFromMob(world, piglin, *current, {piglin.x, piglin.y, piglin.z});
    }
    // parity: `equipItemIfPossible`, which keeps what it picked up for good.
    equipment::set(piglin, slot, stack, true);
    piglin.persistent = true;
    equipmentChanged(piglin, static_cast<std::uint8_t>(slot));
    return true;
}

void MobActionSystem::piglinPicksUp(world::LoadedWorld& world, ServerEntity& piglin, PiglinState& state) {
    const std::int64_t now = world.gameTime();
    // parity: `Mob.ITEM_PICKUP_REACH`, a block across and none up or down.
    const double half = piglin.width() / 2.0;
    for (const world::DroppedItem& item :
         world.state().nearbyItems(piglin.x, piglin.y, piglin.z, 2.0 + piglin.width() + piglin.height())) {
        constexpr double kItemHalf = 0.125;
        if (!item.canBePickedUp() || item.count <= 0 || item.x + kItemHalf < piglin.x - half - 1.0 ||
            item.x - kItemHalf > piglin.x + half + 1.0 || item.z + kItemHalf < piglin.z - half - 1.0 ||
            item.z - kItemHalf > piglin.z + half + 1.0 || item.y + 0.25 < piglin.y ||
            item.y > piglin.y + piglin.height()) {
            continue;
        }
        const ItemStack whole{.item = item.item, .count = item.count, .components = item.components};
        if (!piglinWantsToPickUp(piglin, state, whole, now)) {
            continue;
        }
        // parity: `Piglin.pickUpItem` tells the thrower before `PiglinAi.pickUpItem` takes any of it.
        world::thrownItems::pickedUpByMob(options_.advancementTriggers, world, watchersOf(world), item, item.count,
                                          piglin);
        // parity: `PiglinAi.pickUpItem`.
        state.motion.walk.reset();
        entity::ai::navigation::stop(piglin);
        ItemStack taken = whole;
        if (item.item.path() == "gold_nugget") {
            static_cast<void>(world.state().removeItem(item.entityId));
        } else {
            taken.count = 1;
            if (item.count <= 1) {
                static_cast<void>(world.state().removeItem(item.entityId));
            } else {
                static_cast<void>(world.state().setItemCount(item.entityId, item.count - 1));
            }
        }
        const wire::protocol::play::TakeItemEntityPacket packet{
            .collectedEntityId = item.entityId, .collectorEntityId = piglin.entityId(), .pickupItemCount = taken.count};
        for (PlayerSession* player : watchersOf(world)) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(wire::protocol::play::TakeItemEntityPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        if (itemTagged("minecraft:piglin_loved", taken.item)) {
            state.timeTryingToReachItem.reset();
            if (const std::optional<ItemStack> offhand = equipment::get(piglin, equipment::Slot::OffHand);
                offhand.has_value()) {
                throwFromMob(world, piglin, *offhand, {piglin.x, piglin.y, piglin.z});
            }
            equipment::set(piglin, equipment::Slot::OffHand, taken, true);
            if (taken.item.path() != "gold_ingot") {
                piglin.persistent = true;
            }
            equipmentChanged(piglin, static_cast<std::uint8_t>(equipment::Slot::OffHand));
            state.admiring.setFor(true, now, piglins::kAdmireDuration);
        } else if (itemTagged("minecraft:piglin_food", taken.item) && !state.ateRecently.has(now)) {
            state.ateRecently.setFor(true, now, piglins::kEatCooldown);
        } else if (!piglinEquip(world, piglin, taken)) {
            if (const std::optional<ItemStack> left = piglins::addToInventory(state, taken); left.has_value()) {
                random::RandomSource random = freshRandom();
                const std::optional<entity::ai::randomPos::Spot> spot =
                    entity::ai::randomPos::landPos(world.state(), piglin, random, 4, 2);
                const std::array<double, 3> at = spot.value_or(std::array<double, 3>{piglin.x, piglin.y, piglin.z});
                throwFromMob(world, piglin, *left, {at[0], at[1] + 1.0, at[2]});
            }
        }
        return;
    }
}

void MobActionSystem::piglinStopHoldingOffhand(world::LoadedWorld& world, ServerEntity& piglin, PiglinState& state,
                                               bool barter) {
    const std::optional<ItemStack> held = equipment::get(piglin, equipment::Slot::OffHand);
    equipment::set(piglin, equipment::Slot::OffHand, std::nullopt, false);
    equipmentChanged(piglin, static_cast<std::uint8_t>(equipment::Slot::OffHand));
    if (!held.has_value()) {
        return;
    }
    random::RandomSource random = freshRandom();
    // parity: `throwItems`: towards the nearest player it can see, else somewhere near.
    const auto throwAll = [&](const std::vector<ItemStack>& stacks) {
        if (stacks.empty()) {
            return;
        }
        std::optional<std::array<double, 3>> towards;
        double best = 16.0 * 16.0;
        for (PlayerSession* player : watchersOf(world)) {
            const double dx = player->x - piglin.x;
            const double dy = player->y - piglin.y;
            const double dz = player->z - piglin.z;
            const double distance = (dx * dx) + (dy * dy) + (dz * dz);
            if (!player->dead && player->gamemode != Gamemode::Spectator && distance < best) {
                best = distance;
                towards = std::array<double, 3>{player->x, player->y, player->z};
            }
        }
        if (!towards.has_value()) {
            towards = entity::ai::randomPos::landPos(world.state(), piglin, random, 4, 2)
                          .value_or(std::array<double, 3>{piglin.x, piglin.y, piglin.z});
        }
        swing(piglin);
        for (const ItemStack& stack : stacks) {
            throwFromMob(world, piglin, stack, {(*towards)[0], (*towards)[1] + 1.0, (*towards)[2]});
        }
    };
    if (!isBaby(piglin)) {
        const bool currency = held->item.path() == "gold_ingot";
        if (barter && currency) {
            // parity: `getBarterResponseItems`, the `piglin_bartering` table.
            std::vector<ItemStack> result;
            if (const server::world::loot::LootTable* table =
                    lootTables_->byId(wire::common::Identifier::vanilla("gameplay/piglin_bartering"));
                table != nullptr) {
                server::world::loot::LootContext context = deathLootContext(world, piglin);
                random::RandomSource& roll =
                    table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : random;
                result = table->generate(roll, context.luck, std::nullopt, context);
            }
            throwAll(result);
        } else if (!currency && !piglinEquip(world, piglin, *held)) {
            if (const std::optional<ItemStack> left = piglins::addToInventory(state, *held); left.has_value()) {
                throwAll({*left});
            }
        }
        return;
    }
    if (!piglinEquip(world, piglin, *held)) {
        const std::optional<ItemStack> main = equipment::get(piglin, equipment::Slot::MainHand);
        if (main.has_value()) {
            if (itemTagged("minecraft:piglin_loved", main->item)) {
                static_cast<void>(piglins::addToInventory(state, *main));
            } else {
                throwAll({*main});
            }
        }
        equipment::set(piglin, equipment::Slot::MainHand, held, true);
        piglin.persistent = true;
        equipmentChanged(piglin, static_cast<std::uint8_t>(equipment::Slot::MainHand));
    }
}

void MobActionSystem::piglinSetAngerTarget(world::LoadedWorld& world, ServerEntity& /*piglin*/, PiglinState& state,
                                           std::int32_t targetId) {
    const std::int64_t now = world.gameTime();
    std::optional<wire::common::Uuid> uuid;
    bool hoglin = false;
    for (PlayerSession* player : watchersOf(world)) {
        if (player->entityId() == targetId) {
            if (player->dead || player->gamemode == Gamemode::Creative || player->gamemode == Gamemode::Spectator) {
                return;
            }
            uuid = player->uuid();
        }
    }
    if (!uuid.has_value()) {
        const ServerEntity* target = world.state().entity(targetId);
        if (target == nullptr || target->dying) {
            return;
        }
        uuid = target->uuid();
        hoglin = target->type().path() == "hoglin";
    }
    state.motion.cantReachSince.reset();
    state.angryAt.setFor(*uuid, now, piglins::kAngerDuration);
    if (!hoglin && world.state().entity(targetId) == nullptr &&
        world.gameRules().get(world::gameRules::kUniversalAnger)) {
        state.universalAnger.setFor(true, now, piglins::kAngerDuration);
    }
    if (hoglin && !state.brute && !state.cannotHunt) {
        random::RandomSource random = freshRandom();
        state.huntedRecently.setFor(true, now,
                                    piglins::kTimeBetweenHunts[0] + random.nextInt(piglins::kTimeBetweenHunts[1] -
                                                                                   piglins::kTimeBetweenHunts[0] + 1));
    }
}

void MobActionSystem::piglinBroadcastAnger(world::LoadedWorld& world, ServerEntity& piglin, PiglinState& state,
                                           std::int32_t targetId) {
    static_cast<void>(state);
    const ServerEntity* target = world.state().entity(targetId);
    const bool hoglin = target != nullptr && target->type().path() == "hoglin";
    const bool huntable = hoglin && !(target->sourceNbt.has_value() && target->sourceNbt->getBoolean("CannotBeHunted"));
    const double range = piglin.attributeWithModifiers("minecraft:follow_range");
    for (ServerEntity* other : world.state().entities()) {
        if (other == &piglin || other->removed || other->dying || !piglins::isPiglin(other->type()) || isBaby(*other) ||
            std::abs(other->x - piglin.x) > range || std::abs(other->y - piglin.y) > range ||
            std::abs(other->z - piglin.z) > range) {
            continue;
        }
        PiglinState& theirs = piglins::stateOf(*other);
        if (hoglin && (theirs.brute || theirs.cannotHunt || !huntable)) {
            continue;
        }
        // parity: `setAngerTargetIfCloserThanCurrent`.
        std::optional<std::array<double, 3>> current;
        if (theirs.angryAt.has(world.gameTime())) {
            for (const ServerEntity* candidate : world.state().entities()) {
                if (candidate->uuid() == *theirs.angryAt.value) {
                    current = std::array<double, 3>{candidate->x, candidate->y, candidate->z};
                }
            }
            for (PlayerSession* player : watchersOf(world)) {
                if (player->uuid() == *theirs.angryAt.value) {
                    current = std::array<double, 3>{player->x, player->y, player->z};
                }
            }
        }
        std::array<double, 3> offered{other->x, other->y, other->z};
        if (target != nullptr) {
            offered = {target->x, target->y, target->z};
        } else {
            for (PlayerSession* player : watchersOf(world)) {
                if (player->entityId() == targetId) {
                    offered = {player->x, player->y, player->z};
                }
            }
        }
        const auto distance = [other](const std::array<double, 3>& at) {
            return ((at[0] - other->x) * (at[0] - other->x)) + ((at[1] - other->y) * (at[1] - other->y)) +
                   ((at[2] - other->z) * (at[2] - other->z));
        };
        if (!current.has_value() || distance(offered) < distance(*current)) {
            piglinSetAngerTarget(world, *other, theirs, targetId);
        }
    }
}

void MobActionSystem::piglinWasHurtBy(world::LoadedWorld& world, entity::ai::MobContext& context, PiglinState& state,
                                      std::int32_t attackerId) {
    ServerEntity& piglin = context.mob;
    const std::int64_t now = context.gameTime;
    const ServerEntity* attacker = world.state().entity(attackerId);
    bool player = false;
    for (PlayerSession* candidate : watchersOf(world)) {
        player = player || candidate->entityId() == attackerId;
    }
    if (attacker != nullptr && piglins::isPiglin(attacker->type())) {
        return;
    }
    if (state.brute) {
        // parity: `PiglinBruteAi.wasHurtBy` over `maybeRetaliate`.
        piglinRetaliate(world, piglin, state, attackerId, player);
        return;
    }
    if (equipment::get(piglin, equipment::Slot::OffHand).has_value()) {
        piglinStopHoldingOffhand(world, piglin, state, false);
    }
    state.celebrateLocation.erase();
    state.dancing = false;
    state.admiring.erase();
    if (player) {
        state.admiringDisabled.setFor(true, now, piglins::kHitByPlayerMemory);
    }
    if (state.avoidTarget.has(now)) {
        const ServerEntity* avoided = world.state().entity(*state.avoidTarget.value);
        const std::string_view avoidedType =
            avoided == nullptr ? std::string_view{"player"} : std::string_view{avoided->type().path()};
        const std::string_view attackerType =
            attacker == nullptr ? std::string_view{"player"} : std::string_view{attacker->type().path()};
        if (avoidedType != attackerType) {
            state.avoidTarget.erase();
        }
    }
    if (isBaby(piglin)) {
        state.avoidTarget.setFor(attackerId, now, piglins::kBabyFleeAfterHit);
        piglinBroadcastAnger(world, piglin, state, attackerId);
        return;
    }
    // parity: a hoglin that outnumbers them sends the whole band running.
    std::int32_t piglinsSeen = 1;
    std::int32_t hoglinsSeen = 0;
    for (const ServerEntity* other : context.entities) {
        if (other == &piglin || other->dying || context.distanceSquaredTo(other->x, other->y, other->z) > 16.0 * 16.0) {
            continue;
        }
        if (other->type().path() == "hoglin" && !isBaby(*other)) {
            ++hoglinsSeen;
        } else if (piglins::isPiglin(other->type()) && !isBaby(*other)) {
            ++piglinsSeen;
        }
    }
    if (attacker != nullptr && attacker->type().path() == "hoglin" && hoglinsSeen > piglinsSeen) {
        random::RandomSource random = freshRandom();
        for (ServerEntity* other : world.state().entities()) {
            if (other->type().path() != "piglin" || isBaby(*other) ||
                context.distanceSquaredTo(other->x, other->y, other->z) > 16.0 * 16.0) {
                continue;
            }
            PiglinState& theirs = piglins::stateOf(*other);
            theirs.angryAt.erase();
            other->targetId.reset();
            theirs.motion.walk.reset();
            theirs.avoidTarget.setFor(attackerId, now,
                                      piglins::kRetreatDuration[0] + random.nextInt(piglins::kRetreatDuration[1] -
                                                                                    piglins::kRetreatDuration[0] + 1));
            theirs.huntedRecently.setFor(
                true, now,
                piglins::kTimeBetweenHunts[0] +
                    random.nextInt(piglins::kTimeBetweenHunts[1] - piglins::kTimeBetweenHunts[0] + 1));
        }
        return;
    }
    // parity: `maybeRetaliate`, never while fleeing.
    if (state.activity != piglins::Activity::Avoid) {
        piglinRetaliate(world, piglin, state, attackerId, player);
    }
}

bool MobActionSystem::useOnPiglin(world::LoadedWorld& world, PlayerSession& player, ServerEntity& piglin,
                                  const std::optional<ItemStack>& held) {
    static_cast<void>(player);
    if (!piglins::isPiglin(piglin.type()) || piglins::isBrute(piglin.type())) {
        return false;
    }
    PiglinState& state = piglins::stateOf(piglin);
    const std::int64_t now = world.gameTime();
    // parity: `canAdmire`: an adult, not already admiring, not put off, and a gold ingot.
    if (!held.has_value() || held->item.path() != "gold_ingot" || state.admiringDisabled.has(now) ||
        state.admiring.has(now) || isBaby(piglin)) {
        return false;
    }
    ItemStack taken = *held;
    taken.count = 1;
    if (const std::optional<ItemStack> offhand = equipment::get(piglin, equipment::Slot::OffHand);
        offhand.has_value()) {
        throwFromMob(world, piglin, *offhand, {piglin.x, piglin.y, piglin.z});
    }
    equipment::set(piglin, equipment::Slot::OffHand, taken, true);
    equipmentChanged(piglin, static_cast<std::uint8_t>(equipment::Slot::OffHand));
    state.admiring.setFor(true, now, piglins::kAdmireDuration);
    state.motion.walk.reset();
    entity::ai::navigation::stop(piglin);
    return true;
}

void MobActionSystem::angerNearbyPiglins(world::LoadedWorld& world, const PlayerSession& player, bool onlyIfSeen) {
    static_cast<void>(onlyIfSeen);
    // parity: every idle piglin within sixteen of the player's box.
    for (ServerEntity* other : world.state().entities()) {
        if (other->type().path() != "piglin" || other->dying || other->removed ||
            std::abs(other->x - player.x) > 16.3 || std::abs(other->y - player.y) > 17.8 ||
            std::abs(other->z - player.z) > 16.3) {
            continue;
        }
        PiglinState& state = piglins::stateOf(*other);
        if (state.activity != piglins::Activity::Idle) {
            continue;
        }
        // parity: under `universal_anger`, `setAngerTargetToNearestTargetablePlayerIfFound`.
        const bool universal = world.gameRules().get(world::gameRules::kUniversalAnger);
        piglinSetAngerTarget(world, *other, state,
                             universal ? piglinNearestPlayer(world, *other).value_or(player.entityId())
                                       : player.entityId());
    }
}

void MobActionSystem::dropPiglinInventory(world::LoadedWorld& world, const ServerEntity& piglin) {
    if (piglin.piglin == nullptr) {
        return;
    }
    random::RandomSource random = freshRandom();
    for (const std::optional<ItemStack>& stack : piglin.piglin->inventory) {
        if (!stack.has_value() || stack->count <= 0) {
            continue;
        }
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = stack->item,
                                                   .count = stack->count,
                                                   .x = piglin.x,
                                                   .y = piglin.y,
                                                   .z = piglin.z,
                                                   .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                   .velocityY = 0.2,
                                                   .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                   .pickupDelay = world::kSpawnedPickupDelay,
                                                   .components = stack->components});
    }
}

} // namespace basalt::server::runtime
