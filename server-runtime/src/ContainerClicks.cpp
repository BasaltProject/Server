#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockInteractionHandler.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/VillagerTradeController.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerPick.hpp>
#include <basalt/server/runtime/player/StatAwards.hpp>
#include <basalt/server/runtime/world/BlockPickData.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::MenuSlot;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::protocol::play::ServerboundContainerClickPacket;

/// parity: the click target vanilla uses for "outside the window", which throws the cursor stack.
constexpr std::int32_t kOutsideWindowSlot = -999;
constexpr std::int32_t kRightButton = 1;

// parity: `AbstractContainerMenu`'s own quick-craft stages and drag types.
constexpr std::int32_t kQuickCraftStart = 0;
constexpr std::int32_t kQuickCraftAdd = 1;
constexpr std::int32_t kQuickCraftEnd = 2;
constexpr std::int32_t kQuickCraftTypeLeft = 0;
constexpr std::int32_t kQuickCraftTypeRight = 1;
constexpr std::int32_t kQuickCraftTypeMiddle = 2;

// parity: `Player.drop`'s own throw.
constexpr double kThrowSpeed = 0.3;
constexpr double kThrowLift = 0.1;
constexpr double kThrowEyeHeight = 1.5;
/// parity: `createItemStackToDrop`, a fiftieth of a block of sideways spread.
constexpr double kTossSpread = 0.02;

std::optional<Identifier> parsedOrNothing(const std::string& id) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

std::string_view nameOf(ServerboundContainerClickPacket::Mode mode) {
    switch (mode) {
    case ServerboundContainerClickPacket::Mode::Pickup:
        return "pickup";
    case ServerboundContainerClickPacket::Mode::QuickMove:
        return "quick_move";
    case ServerboundContainerClickPacket::Mode::Swap:
        return "swap";
    case ServerboundContainerClickPacket::Mode::Clone:
        return "clone";
    case ServerboundContainerClickPacket::Mode::Throw:
        return "throw";
    case ServerboundContainerClickPacket::Mode::QuickCraft:
        return "quick_craft";
    case ServerboundContainerClickPacket::Mode::PickupAll:
        return "pickup_all";
    }
    return "pickup";
}

storage::StoredItemStack withCount(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

bool sameItem(const storage::StoredItemStack& left, const storage::StoredItemStack& right) {
    return left.id == right.id && left.components == right.components;
}

} // namespace

void ContainerMenuController::handleContainerClick(const ServerboundContainerClickPacket& packet) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    if (packet.containerId != openMenu.containerId()) {
        sendInventory();
        return;
    }
    const std::int32_t slotIndex = packet.slot;
    const std::int32_t button = packet.button;
    if (hooks_.clickAllowed) {
        const std::optional<MenuSlot> clickedSlot = openMenu.slotAt(slotIndex);
        std::optional<std::string> item;
        if (clickedSlot.has_value()) {
            if (const auto* inventory = std::get_if<player::menuSlot::Inventory>(&*clickedSlot); inventory != nullptr) {
                const std::optional<storage::StoredItemStack>& stack = session->inventory.at(inventory->index);
                if (stack.has_value()) {
                    item = stack->id;
                }
            }
        }
        if (!hooks_.clickAllowed(slotIndex, button, nameOf(packet.mode), item)) {
            sendInventory();
            return;
        }
    }

    // A plugin's window: a click is a button press, told as the plugin's slot, never the player's own.
    if (pluginMenu_.has_value()) {
        const std::optional<MenuSlot> clicked = openMenu.slotAt(slotIndex);
        if (clicked.has_value() && pluginMenuClick_) {
            if (const auto* block = std::get_if<player::menuSlot::Block>(&*clicked); block != nullptr) {
                pluginMenuClick_(block->index);
            }
        }
        sendInventory();
        return;
    }

    switch (packet.mode) {
    case ServerboundContainerClickPacket::Mode::Pickup: {
        if (slotIndex == kOutsideWindowSlot) {
            if (!carriedItem_.has_value()) {
                sendInventory();
                return;
            }
            const storage::StoredItemStack carried = *carriedItem_;
            const std::int32_t thrown = button == kRightButton ? 1 : carried.count;
            dropStack(*session, withCount(carried, thrown));
            player::statAwards::awardThrown(*session, carried.id, thrown);
            const std::int32_t left = carried.count - thrown;
            carriedItem_ = left <= 0 ? std::nullopt : std::optional{withCount(carried, left)};
            sendInventory();
            return;
        }
        const std::optional<MenuSlot> slot = openMenu.slotAt(slotIndex);
        if (!slot.has_value()) {
            sendInventory();
            return;
        }
        if (std::holds_alternative<player::menuSlot::Result>(*slot)) {
            takeCraftingResult(*session);
        } else {
            pickupClick(*session, *slot, button == kRightButton);
        }
        break;
    }
    case ServerboundContainerClickPacket::Mode::Swap: {
        const std::optional<MenuSlot> slot = openMenu.slotAt(slotIndex);
        if (!slot.has_value() || !canPlaceInto(*slot) || button < 0 || button >= PlayerInventory::kHotbarSize) {
            sendInventory();
            return;
        }
        const MenuSlot hotbar = player::menuSlot::Inventory{PlayerInventory::kHotbarStart + button};
        // parity: `AbstractContainerMenu.doClick`'s swap asks the slot's `mayPlace` and its size.
        if (const std::optional<storage::StoredItemStack> incoming = readSlot(*session, hotbar);
            incoming.has_value() && brewingStandOpen() &&
            (!accepts(*slot, *incoming) || incoming->count > slotCapacity(*slot, *incoming))) {
            sendInventory();
            return;
        }
        const std::optional<storage::StoredItemStack> fromSlot = readSlot(*session, *slot);
        writeSlot(*session, *slot, readSlot(*session, hotbar));
        writeSlot(*session, hotbar, fromSlot);
        break;
    }
    case ServerboundContainerClickPacket::Mode::Throw: {
        const std::optional<MenuSlot> slot = openMenu.slotAt(slotIndex);
        if (!slot.has_value()) {
            sendInventory();
            return;
        }
        if (std::holds_alternative<player::menuSlot::Result>(*slot)) {
            const std::optional<storage::StoredItemStack> previouslyCarried = carriedItem_;
            carriedItem_.reset();
            takeCraftingResult(*session);
            if (carriedItem_.has_value()) {
                dropStack(*session, *carriedItem_);
                player::statAwards::awardThrown(*session, carriedItem_->id, carriedItem_->count);
            }
            carriedItem_ = previouslyCarried;
            break;
        }
        const std::optional<storage::StoredItemStack> stack = readSlot(*session, *slot);
        if (!stack.has_value()) {
            sendInventory();
            return;
        }
        const std::int32_t thrown = button == 0 ? 1 : stack->count;
        dropStack(*session, withCount(*stack, thrown));
        player::statAwards::awardThrown(*session, stack->id, thrown);
        const std::int32_t left = stack->count - thrown;
        writeSlot(*session, *slot, left <= 0 ? std::nullopt : std::optional{withCount(*stack, left)});
        break;
    }
    case ServerboundContainerClickPacket::Mode::QuickMove: {
        const std::optional<MenuSlot> slot = openMenu.slotAt(slotIndex);
        if (!slot.has_value()) {
            sendInventory();
            return;
        }
        quickMove(*session, *slot);
        break;
    }
    case ServerboundContainerClickPacket::Mode::QuickCraft:
        // Mid-gesture stages answer with nothing: resending while the player drags makes the drag snap back.
        if (!handleQuickCraft(*session, packet)) {
            return;
        }
        break;
    case ServerboundContainerClickPacket::Mode::Clone: {
        // parity: creative's own middle-click, and only with an empty cursor.
        if (session->gamemode != Gamemode::Creative || carriedItem_.has_value()) {
            sendInventory();
            return;
        }
        const std::optional<MenuSlot> slot = openMenu.slotAt(slotIndex);
        const std::optional<storage::StoredItemStack> stack =
            slot.has_value() ? readSlot(*session, *slot) : std::nullopt;
        if (!stack.has_value()) {
            sendInventory();
            return;
        }
        carriedItem_ = withCount(*stack, PlayerInventory::maxStackIn(*stack));
        break;
    }
    case ServerboundContainerClickPacket::Mode::PickupAll:
        collectMatching(*session, button != 0);
        break;
    }
    sendInventory();
}

void ContainerMenuController::collectMatching(PlayerSession& session, bool reversed) {
    if (!carriedItem_.has_value()) {
        return;
    }
    const storage::StoredItemStack carried = *carriedItem_;
    const std::int32_t most = PlayerInventory::maxStackIn(carried);
    std::int32_t total = carried.count;
    if (total >= most) {
        return;
    }

    // Every slot of the open window, not just the inventory: vanilla's sweep gathers from all that the player sees.
    std::vector<MenuSlot> slots;
    for (std::int32_t index = 0; index < openMenu.slotCount(); index++) {
        const std::optional<MenuSlot> slot = openMenu.slotAt(index);
        if (slot.has_value() && !std::holds_alternative<player::menuSlot::Result>(*slot)) {
            slots.push_back(*slot);
        }
    }
    if (reversed) {
        std::ranges::reverse(slots);
    }

    // The first pass skips full stacks so loose ones are consumed first, which makes double-clicking tidy.
    for (std::int32_t pass = 0; pass < 2; pass++) {
        for (const MenuSlot& slot : slots) {
            if (total >= most) {
                break;
            }
            const std::optional<storage::StoredItemStack> existing = readSlot(session, slot);
            if (!existing.has_value() || !sameItem(*existing, carried)) {
                continue;
            }
            if (pass == 0 && existing->count >= PlayerInventory::maxStackIn(*existing)) {
                continue;
            }
            const std::int32_t taken = std::min(most - total, existing->count);
            total += taken;
            const std::int32_t left = existing->count - taken;
            writeSlot(session, slot, left <= 0 ? std::nullopt : std::optional{withCount(*existing, left)});
        }
    }
    carriedItem_ = withCount(carried, total);
}

bool ContainerMenuController::handleQuickCraft(PlayerSession& session, const ServerboundContainerClickPacket& packet) {
    // parity: `getQuickcraftHeader`.
    const std::int32_t stage = packet.button & 3;
    const std::int32_t type = (packet.button >> 2) & 3;

    if (stage == kQuickCraftStart) {
        quickCraftType_ = type;
        quickCraftSlots_.clear();
        quickCraftActive_ = carriedItem_.has_value();
        return false;
    }
    if (stage == kQuickCraftAdd && quickCraftActive_) {
        const std::optional<MenuSlot> slot = openMenu.slotAt(packet.slot);
        if (!slot.has_value() || !canPlaceInto(*slot)) {
            return false;
        }
        const std::optional<storage::StoredItemStack> existing = readSlot(session, *slot);
        const bool stackable =
            carriedItem_.has_value() && (!existing.has_value() || sameItem(*existing, *carriedItem_));
        if (stackable) {
            quickCraftSlots_.push_back(packet.slot);
        }
        return false;
    }
    if (stage == kQuickCraftEnd && quickCraftActive_) {
        distributeQuickCraft(session);
        quickCraftActive_ = false;
        quickCraftSlots_.clear();
        return true;
    }
    quickCraftActive_ = false;
    quickCraftSlots_.clear();
    return true;
}

void ContainerMenuController::distributeQuickCraft(PlayerSession& session) {
    if (!carriedItem_.has_value()) {
        return;
    }
    const storage::StoredItemStack carried = *carriedItem_;
    std::vector<MenuSlot> targets;
    for (const std::int32_t index : quickCraftSlots_) {
        const std::optional<MenuSlot> slot = openMenu.slotAt(index);
        if (slot.has_value() && canPlaceInto(*slot)) {
            targets.push_back(*slot);
        }
    }
    if (targets.empty()) {
        return;
    }

    // parity: a one-slot drag is just a click on that slot, cursor rules and all.
    if (targets.size() == 1) {
        pickupClick(session, targets.front(), quickCraftType_ == kQuickCraftTypeRight);
        return;
    }

    std::int32_t perSlot = PlayerInventory::maxStackIn(carried);
    if (quickCraftType_ == kQuickCraftTypeLeft) {
        perSlot = carried.count / static_cast<std::int32_t>(targets.size());
    } else if (quickCraftType_ == kQuickCraftTypeRight) {
        perSlot = 1;
    }
    if (perSlot <= 0) {
        return;
    }

    std::int32_t remaining = carried.count;
    for (const MenuSlot& slot : targets) {
        if (remaining <= 0) {
            break;
        }
        const std::optional<storage::StoredItemStack> existing = readSlot(session, slot);
        const std::int32_t already = existing.has_value() ? existing->count : 0;
        const std::int32_t give = std::min({perSlot, slotCapacity(slot, carried) - already, remaining});
        if (give <= 0) {
            continue;
        }
        writeSlot(session, slot, withCount(carried, already + give));
        remaining -= give;
    }
    if (quickCraftType_ == kQuickCraftTypeMiddle && session.gamemode == Gamemode::Creative) {
        carriedItem_ = carried;
    } else if (remaining <= 0) {
        carriedItem_.reset();
    } else {
        carriedItem_ = withCount(carried, remaining);
    }
}

void ContainerMenuController::pickupClick(PlayerSession& session, const MenuSlot& slot, bool half) {
    if (!mayPickup(slot)) {
        return;
    }
    const std::optional<storage::StoredItemStack> inSlot = readSlot(session, slot);
    if (!carriedItem_.has_value()) {
        if (!inSlot.has_value()) {
            return;
        }
        const std::int32_t taken = half ? (inSlot->count + 1) / 2 : inSlot->count;
        carriedItem_ = withCount(*inSlot, taken);
        const std::int32_t left = inSlot->count - taken;
        writeSlot(session, slot, left <= 0 ? std::nullopt : std::optional{withCount(*inSlot, left)});
        return;
    }
    if (!canPlaceInto(slot)) {
        return;
    }
    const storage::StoredItemStack carried = *carriedItem_;
    // parity: `Slot.mayPlace` and `getMaxStackSize`.
    const std::int32_t capacity = slotCapacity(slot, carried);
    if ((brewingStandOpen() || mountOpen()) && !accepts(slot, carried)) {
        return;
    }
    if (!inSlot.has_value()) {
        const std::int32_t placed = std::min(half ? 1 : carried.count, capacity);
        writeSlot(session, slot, withCount(carried, placed));
        const std::int32_t left = carried.count - placed;
        carriedItem_ = left <= 0 ? std::nullopt : std::optional{withCount(carried, left)};
        return;
    }
    if (sameItem(*inSlot, carried)) {
        const std::int32_t room = capacity - inSlot->count;
        if (room <= 0) {
            return;
        }
        const std::int32_t moved = half ? std::min({1, room, carried.count}) : std::min(room, carried.count);
        writeSlot(session, slot, withCount(*inSlot, inSlot->count + moved));
        const std::int32_t left = carried.count - moved;
        carriedItem_ = left <= 0 ? std::nullopt : std::optional{withCount(carried, left)};
        return;
    }
    if (carried.count > capacity) {
        return;
    }
    writeSlot(session, slot, carried);
    carriedItem_ = inSlot;
}

void ContainerMenuController::quickMove(PlayerSession& session, const MenuSlot& slot) {
    if (std::holds_alternative<player::menuSlot::Result>(slot)) {
        const std::optional<storage::StoredItemStack> previouslyCarried = carriedItem_;
        carriedItem_.reset();
        while (craftingResult_.has_value()) {
            const storage::StoredItemStack batch = *craftingResult_;
            if (session.inventory.add(batch) > 0) {
                break;
            }
            carriedItem_.reset();
            takeCraftingResult(session);
            carriedItem_.reset();
        }
        carriedItem_ = previouslyCarried;
        return;
    }

    // parity: `MerchantMenu.quickMoveStack`.
    const auto* merchantSlot = std::get_if<player::menuSlot::Block>(&slot);
    if (openMerchant_ && merchantSlot != nullptr && merchantSlot->index == VillagerTradeController::kResult) {
        owner_->trades().quickMoveResult(session);
        return;
    }
    const std::optional<storage::StoredItemStack> stack = readSlot(session, slot);
    if (!stack.has_value()) {
        return;
    }
    // parity: `AbstractContainerMenu.quickMoveStack`.
    if (mountOpen()) {
        quickMoveMount(session, slot, *stack);
        return;
    }
    const auto* fromInventory = std::get_if<player::menuSlot::Inventory>(&slot);
    if (fromInventory != nullptr && brewingStandOpen()) {
        quickMoveIntoBrewingStand(session, slot, *stack);
        return;
    }
    std::vector<MenuSlot> targets;
    if (fromInventory != nullptr && hasBlockSlots() && !openMerchant_) {
        for (std::int32_t index = 0; index < openBlockSize; index++) {
            targets.emplace_back(player::menuSlot::Block{index});
        }
    } else if (fromInventory == nullptr) {
        for (std::int32_t index = PlayerInventory::kHotbarStart; index < PlayerInventory::kMainSlots; index++) {
            targets.emplace_back(player::menuSlot::Inventory{index});
        }
        for (std::int32_t index = 0; index < PlayerInventory::kHotbarStart; index++) {
            targets.emplace_back(player::menuSlot::Inventory{index});
        }
    } else if (fromInventory->index < PlayerInventory::kHotbarStart) {
        for (std::int32_t index = PlayerInventory::kHotbarStart; index < PlayerInventory::kMainSlots; index++) {
            targets.emplace_back(player::menuSlot::Inventory{index});
        }
    } else {
        for (std::int32_t index = 0; index < PlayerInventory::kHotbarStart; index++) {
            targets.emplace_back(player::menuSlot::Inventory{index});
        }
    }

    std::int32_t remaining = stack->count;
    // parity: `moveItemStackTo` stacks onto what is already there first.
    for (const MenuSlot& target : targets) {
        if (remaining <= 0) {
            break;
        }
        if (!accepts(target, *stack)) {
            continue;
        }
        const std::optional<storage::StoredItemStack> existing = readSlot(session, target);
        if (!existing.has_value() || !sameItem(*existing, *stack)) {
            continue;
        }
        const std::int32_t room = slotCapacity(target, *existing) - existing->count;
        if (room <= 0) {
            continue;
        }
        const std::int32_t moved = std::min(room, remaining);
        writeSlot(session, target, withCount(*existing, existing->count + moved));
        remaining -= moved;
    }
    for (const MenuSlot& target : targets) {
        if (remaining <= 0) {
            break;
        }
        if (!accepts(target, *stack) || readSlot(session, target).has_value()) {
            continue;
        }
        const std::int32_t placed = std::min(remaining, slotCapacity(target, *stack));
        writeSlot(session, target, withCount(*stack, placed));
        remaining -= placed;
        break;
    }
    writeSlot(session, slot, remaining <= 0 ? std::nullopt : std::optional{withCount(*stack, remaining)});
}

void ContainerMenuController::dropStack(PlayerSession& session, const storage::StoredItemStack& stack) {
    const std::optional<Identifier> item = parsedOrNothing(stack.id);
    if (!item.has_value()) {
        return;
    }
    const double yaw = static_cast<double>(session.yaw) * std::numbers::pi / 180.0;
    const double pitch = static_cast<double>(session.pitch) * std::numbers::pi / 180.0;
    const double roll = options_.nextDouble ? options_.nextDouble() : 0.0;
    const double spreadRoll = options_.nextDouble ? options_.nextDouble() : 0.0;
    const double tossAngle = roll * 2.0 * std::numbers::pi;
    const double tossSpread = kTossSpread * spreadRoll;
    owner_->spawnDroppedItem(*item, stack.count, session.x, session.y + kThrowEyeHeight, session.z,
                             (-std::sin(yaw) * std::cos(pitch) * kThrowSpeed) + (std::cos(tossAngle) * tossSpread),
                             (-std::sin(pitch) * kThrowSpeed) + kThrowLift + owner_->blockInteraction().tossLift(),
                             (std::cos(yaw) * std::cos(pitch) * kThrowSpeed) + (std::sin(tossAngle) * tossSpread));
}

void ContainerMenuController::handleSetCreativeModeSlot(
    const wire::protocol::play::ServerboundSetCreativeModeSlotPacket& packet) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr || session->gamemode != Gamemode::Creative) {
        return;
    }
    // The client numbers container 0's slots its own way; slot -1 and the crafting slots resolve to nothing.
    const std::optional<std::int32_t> slot = PlayerInventory::storageSlotOf(packet.slot);
    if (!slot.has_value()) {
        return;
    }
    std::optional<storage::StoredItemStack> stack;
    if (packet.item.has_value()) {
        const server::registry::Item* item = server::registry::items::registry().byId(packet.item->itemId);
        if (item != nullptr) {
            // A creative client echoes a slot when the inventory opens, so what is not rebuilt here is stripped.
            world::RealEnchantments* enchantments = owner_->options().enchantments;
            stack = storage::StoredItemStack{
                .id = item->id.toString(),
                .count = packet.item->count,
                .components = item::itemComponentCodecs::toStored(
                    packet.item->components, [enchantments](std::int32_t protocolId) -> const Identifier* {
                        if (enchantments == nullptr || protocolId < 0 ||
                            static_cast<std::size_t>(protocolId) >= enchantments->size()) {
                            return nullptr;
                        }
                        return &enchantments->entries()[static_cast<std::size_t>(protocolId)].first;
                    })};
        }
    }
    // parity: `handleSetCreativeModeSlot`'s `validData`.
    if (stack.has_value() &&
        stack->count > server::registry::itemStacking::maxStackSize(stack->id, stack->components)) {
        return;
    }
    session->inventory.set(*slot, stack);
    sendInventorySlot(*slot);
}

void ContainerMenuController::handlePickItemFromBlock(const wire::protocol::play::PickItemFromBlockPacket& packet) {
    PlayerSession* session = owner_->playerSession();
    if (session == nullptr) {
        return;
    }
    const std::optional<std::int32_t> stateId = owner_->world().blockState(packet.pos.x, packet.pos.y, packet.pos.z);
    if (!stateId.has_value() || owner_->blockInteraction().isAirLike(*stateId)) {
        return;
    }
    const std::optional<server::registry::BlockState> block = server::registry::blocks::byStateId(*stateId);
    if (!block.has_value()) {
        return;
    }
    // parity: `Block.asItem`, which is not the block's own name for dust, crops or cocoa.
    const server::registry::Item* item =
        server::registry::items::registry().byName(server::registry::itemBlocks::itemOf(block->block));
    if (item == nullptr) {
        return;
    }
    if (hooks_.pickAllowed &&
        !hooks_.pickAllowed(*session, packet.pos.x, packet.pos.y, packet.pos.z, item->id.toString())) {
        return;
    }
    // parity: `handlePickItemFromBlock`.
    const bool includeData = session->gamemode == Gamemode::Creative && packet.includeData;
    const server::world::ChunkBlockEntity* entity = nullptr;
    if (world::LoadedWorld* world = owner_->currentWorld(); world != nullptr) {
        if (const server::world::Chunk* chunk = world->state().chunkIfLoaded(packet.pos.x >> 4, packet.pos.z >> 4);
            chunk != nullptr) {
            entity = chunk->blockEntityAt(packet.pos.x & 15, packet.pos.y, packet.pos.z & 15);
        }
    }
    storage::StoredItemStack picked{.id = item->id.toString(), .count = 1, .components = {}};
    world::blockPickData::addCloneComponents(*stateId, entity, includeData, picked.components);
    if (includeData && entity != nullptr) {
        world::blockPickData::addBlockData(*entity, picked.components);
    }
    pickItem(*session, picked);
}

void ContainerMenuController::handlePickItemFromEntity(const wire::protocol::play::PickItemFromEntityPacket& packet) {
    PlayerSession* session = owner_->playerSession();
    world::LoadedWorld* world = owner_->currentWorld();
    if (session == nullptr || world == nullptr) {
        return;
    }
    const entity::ServerEntity* target = world->state().entity(packet.entityId);
    if (target == nullptr || target->removed || !owner_->withinAttackReach(*session, *target)) {
        return;
    }
    // parity: `Entity.getPickResult`.
    const std::optional<Identifier> item = entity::entityPickResults::itemFor(target->type());
    if (!item.has_value()) {
        return;
    }
    pickItem(*session, storage::StoredItemStack{.id = item->toString(), .count = 1, .components = {}});
}

void ContainerMenuController::pickItem(PlayerSession& session, const storage::StoredItemStack& picked) {
    const bool changed = player::pickItemFor(session, picked);
    // parity: `tryPickItem` names the selected slot whether or not anything was found.
    if (session.connection().setHeldSlot) {
        session.connection().setHeldSlot(session.inventory.heldSlot());
    }
    if (changed) {
        sendInventoryAll();
    }
}

} // namespace basalt::server::runtime
