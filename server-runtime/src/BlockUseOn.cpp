#include "BlockUseOnShared.hpp"

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/ItemUseTimers.hpp>
#include <basalt/server/runtime/MapSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/WitherSystem.hpp>
#include <basalt/server/runtime/item/ItemWear.hpp>
#include <basalt/server/runtime/player/PlayerBeehives.hpp>
#include <basalt/server/runtime/player/PlayerBlockItemUse.hpp>
#include <basalt/server/runtime/player/PlayerBlockPlace.hpp>
#include <basalt/server/runtime/player/PlayerBlockToggle.hpp>
#include <basalt/server/runtime/player/PlayerBuckets.hpp>
#include <basalt/server/runtime/player/PlayerCakesAndPots.hpp>
#include <basalt/server/runtime/player/PlayerCauldrons.hpp>
#include <basalt/server/runtime/player/PlayerFireworks.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSpawnItems.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/Beehives.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/BlockTransformers.hpp>
#include <basalt/server/runtime/world/BoneMeal.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/CommandBlocks.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>
#include <basalt/server/runtime/world/EndPortals.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/NetherPortals.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>
#include <basalt/server/runtime/world/Rails.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/RedstoneWire.hpp>
#include <basalt/server/runtime/world/RespawnAnchors.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Shulkers.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/Waxing.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
using namespace blockUseOn;

void BlockInteractionHandler::primeTntAt(PlayerSession& session, const BlockPos& pos, std::int32_t heldSlot,
                                         const storage::StoredItemStack& held, const bool wears) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::primeTntFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held, wears);
    }
}

void BlockInteractionHandler::igniteAt(PlayerSession& session, const BlockPos& pos, std::int32_t heldSlot,
                                       const storage::StoredItemStack& held) {
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        player::igniteFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
    }
}

bool BlockInteractionHandler::useBoneMeal(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                          const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr &&
           player::useBoneMealFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
}

bool BlockInteractionHandler::useHoneycomb(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                           const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr &&
           player::useHoneycombFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
}

bool BlockInteractionHandler::useAxe(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                     const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    return world != nullptr && player::useAxeFor(breakContext(), session, *world, pos.x, pos.y, pos.z, heldSlot, held);
}

bool BlockInteractionHandler::chargeAnchor(PlayerSession& session, const BlockPos& pos, const std::int32_t heldSlot,
                                           const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    const std::optional<world::BlockChange> charged =
        world == nullptr ? std::nullopt : world::respawnAnchors::charge(world->state(), pos.x, pos.y, pos.z);
    if (!charged.has_value()) {
        return false;
    }
    broadcastBlockUpdate(pos, charged->stateId);
    // parity: `setBlock(pos, state, 3)`.
    notifyNeighbours(pos);
    heardBySculk("block_change", pos);
    if (session.gamemode == Gamemode::Creative) {
        return true;
    }
    session.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
    owner_->sendInventorySlot(heldSlot);
    return true;
}

void BlockInteractionHandler::placeEnderEye(PlayerSession& session, const BlockPos& pos, std::int32_t heldSlot,
                                            const storage::StoredItemStack& held) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    const std::vector<world::BlockChange> changed = world::endPortals::placeEye(world->state(), pos.x, pos.y, pos.z);
    if (changed.empty()) {
        return;
    }
    for (const world::BlockChange& change : changed) {
        broadcastBlockUpdate(BlockPos{change.x, change.y, change.z}, change.stateId);
    }
    if (session.gamemode == Gamemode::Creative) {
        return;
    }
    session.inventory.set(heldSlot, held.count <= 1 ? std::nullopt : std::optional{withCount(held, held.count - 1)});
    owner_->sendInventorySlot(heldSlot);
}

void BlockInteractionHandler::awardItemUsedOn(const PlayerSession& session, const BlockPos& pos,
                                              const std::optional<std::int32_t>& clickedState,
                                              const std::optional<storage::StoredItemStack>& tool) const {
    awardBlockUse(session, pos, clickedState, tool, false);
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    world::LoadedWorld* world = owner_->currentWorld();
    world::RealLootTables* tables = owner_->options().lootTables;
    if (triggers == nullptr || world == nullptr || tables == nullptr || !clickedState.has_value() ||
        !triggers->listensTo("item_used_on_block")) {
        return;
    }
    triggers->fire("item_used_on_block", session.name(), session.uuid().toString(),
                   world::TriggerEvent{.location = world::triggerFacts::atBlock(*tables, *world, session, pos.x, pos.y,
                                                                                pos.z, tool, clickedState)});
}

void BlockInteractionHandler::awardBlockUse(const PlayerSession& session, const BlockPos& pos,
                                            const std::optional<std::int32_t>& clickedState,
                                            const std::optional<storage::StoredItemStack>& tool,
                                            bool withoutItem) const {
    world::AdvancementTriggers* triggers = owner_->options().advancementTriggers;
    world::LoadedWorld* world = owner_->currentWorld();
    world::RealLootTables* tables = owner_->options().lootTables;
    const std::string_view trigger = withoutItem ? "default_block_use" : "any_block_use";
    if (triggers == nullptr || world == nullptr || tables == nullptr || !clickedState.has_value() ||
        !triggers->listensTo(trigger)) {
        return;
    }
    triggers->fire(trigger, session.name(), session.uuid().toString(),
                   world::TriggerEvent{.location = world::triggerFacts::atBlock(*tables, *world, session, pos.x, pos.y,
                                                                                pos.z, tool, clickedState)});
}

void BlockInteractionHandler::handleUseItem(const play::ServerboundUseItemPacket& useItem) {
    // The ack comes first and unconditionally: it is what undoes the client's prediction when the server disagrees.
    owner_->sendCompressed(play::BlockChangedAckPacket::kId, [&useItem](wire::io::ByteBuffer& buffer) {
        play::BlockChangedAckPacket{.sequence = useItem.sequence}.write(buffer);
    });
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr || session->gamemode == Gamemode::Spectator) {
        return;
    }
    if (hooks_.airUseAllowed) {
        const std::optional<storage::StoredItemStack> inHand =
            useItem.hand == play::Hand::MainHand ? session->inventory.heldItem()
                                                 : session->inventory.at(PlayerInventory::kOffhandSlot);
        if (!hooks_.airUseAllowed(*session, inHand.has_value() ? inHand->id : "",
                                  useItem.hand == play::Hand::MainHand)) {
            owner_->sendInventory();
            return;
        }
    }
    if (useItem.hand != play::Hand::MainHand) {
        const std::optional<storage::StoredItemStack> offhand = session->inventory.at(PlayerInventory::kOffhandSlot);
        const std::optional<Identifier> other = offhand.has_value() ? parsedOrNothing(offhand->id) : std::nullopt;
        if (!other.has_value() || owner_->itemUseTimers().onCooldown(*other)) {
            return;
        }
        // parity: `CrossbowItem.use` and `BowItem.use` answer either hand.
        if (owner_->itemUseTimers().startDrawing(*session, *other, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `FishingRodItem.use` answers either hand.
        if (owner_->options().fishing != nullptr &&
            owner_->options().fishing->useRod(breakContext(), *session, *world, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `EmptyMapItem.use` answers either hand.
        if (owner_->options().maps != nullptr &&
            owner_->options().maps->useEmptyMap(breakContext(), *session, *world, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `EnderEyeItem.use` answers either hand.
        if (world::enderEyes::throwFor(breakContext(), *session, *world, PlayerInventory::kOffhandSlot)) {
            return;
        }
        // parity: `Item.use` charges a `kinetic_weapon` from either hand.
        if (owner_->combat().startCharge(*session, PlayerInventory::kOffhandSlot)) {
            return;
        }
        if (!owner_->itemUseTimers().startBlocking(*session, *other, PlayerInventory::kOffhandSlot)) {
            static_cast<void>(owner_->itemUseTimers().startSpyglass(*session, *other, PlayerInventory::kOffhandSlot));
        }
        return;
    }
    const std::int32_t heldSlot = PlayerInventory::kHotbarStart + session->inventory.heldSlot();
    const std::optional<storage::StoredItemStack> held = session->inventory.at(heldSlot);
    const std::optional<Identifier> item = held.has_value() ? parsedOrNothing(held->id) : std::nullopt;
    if (!item.has_value()) {
        return;
    }
    // parity: `FireworkRocketItem.use` on a glider lets go of every lead first, snapping.
    if (session->fallFlying && item->path() == "firework_rocket" && !owner_->itemUseTimers().onCooldown(*item) &&
        owner_->options().mobs != nullptr && owner_->options().mobs->dropAllLeashes(*world, *session)) {
        owner_->broadcastSound("minecraft:entity.lead.break", session->x, session->y, session->z,
                               play::SoundSource::Neutral);
    }
    // parity: `FoodOnAStickItem.use`, the boost its mount takes and the wear it costs.
    if (session->vehicleId.has_value() && owner_->options().mobs != nullptr &&
        (item->path() == "carrot_on_a_stick" || item->path() == "warped_fungus_on_a_stick")) {
        entity::ServerEntity* vehicle = world->state().entity(*session->vehicleId);
        if (vehicle != nullptr && owner_->options().mobs->boostSteerable(*vehicle, item->path())) {
            wearHeldItem(item->path() == "carrot_on_a_stick" ? 7 : 1);
            return;
        }
    }
    // parity: `ItemCooldowns.isOnCooldown`.
    if (owner_->itemUseTimers().onCooldown(*item) || owner_->itemUseTimers().startEating(*session, *item, heldSlot) ||
        owner_->itemUseTimers().startDrawing(*session, *item, heldSlot) ||
        owner_->itemUseTimers().throwEnderPearl(*session, *item, heldSlot) ||
        world::enderEyes::throwFor(breakContext(), *session, *world, heldSlot) ||
        (owner_->options().fireworks != nullptr &&
         player::useRocketFor(breakContext(), *owner_->options().fireworks, *session, *world, heldSlot)) ||
        owner_->itemUseTimers().startBlocking(*session, *item, heldSlot) ||
        owner_->combat().startCharge(*session, heldSlot) ||
        owner_->itemUseTimers().startSpyglass(*session, *item, heldSlot) ||
        (owner_->options().fishing != nullptr &&
         owner_->options().fishing->useRod(breakContext(), *session, *world, heldSlot)) ||
        (owner_->options().maps != nullptr &&
         owner_->options().maps->useEmptyMap(breakContext(), *session, *world, heldSlot)) ||
        !world::buckets::isBucket(*item)) {
        return;
    }
    // parity: `BucketItem.use` asks `mayUseItemAt`, which an adventure player never may.
    if (session->gamemode == Gamemode::Adventure) {
        return;
    }

    static_cast<void>(player::useBucketFor(breakContext(), *session, *world, *item, heldSlot, *held, useItem.yaw,
                                           useItem.pitch, hooks_.bucketAllowed));
}

} // namespace basalt::server::runtime
