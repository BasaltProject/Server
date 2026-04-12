#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::MenuSlot;
using player::PlayerInventory;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::BlockPos;
using wire::protocol::play::ContainerSetDataPacket;
using wire::protocol::play::OpenScreenPacket;

[[nodiscard]] std::optional<Identifier> itemOf(const storage::StoredItemStack& stack) {
    const std::expected<Identifier, wire::common::IdentifierError> parsed = Identifier::parse(stack.id);
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

[[nodiscard]] storage::StoredItemStack counted(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

[[nodiscard]] bool placeable(std::int32_t slot, const Identifier& item) {
    return world::containers::acceptsThroughFace(world::brewingStands::block().path(), slot, item, true);
}

} // namespace

void ContainerMenuController::openBrewingStand(PlayerSession& session, const BlockPos& pos) {
    if (hooks_.openAllowed && !hooks_.openAllowed("brewing_stand", pos)) {
        return;
    }
    returnCraftingGrid(session);
    openMenu = Menu::brewingStand(allocateContainerId());
    openBlock = pos;
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_.reset();
    openStonecutter_ = false;
    openSmithing_ = false;
    openAnvil_ = false;
    openGrindstone_ = false;
    openEnchantment_ = false;
    openBeacon_ = false;
    openLoom_ = false;
    openBlockSize = world::brewingStands::kSize;
    openBlockData_ = {-1, -1, -1, -1};
    std::ranges::fill(shownBlockSlots_, std::nullopt);
    owner_->sendCompressed(OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        OpenScreenPacket{.containerId = openMenu.containerId(),
                         .menuType = wire::protocol::play::menuType::kBrewingStand,
                         .title = wire::text::Component::translatable("container.brewing")}
            .write(buffer);
    });
    sendInventory();
    sendBrewingBars();
}

bool ContainerMenuController::brewingStandOpen() const {
    world::LoadedWorld* world = owner_->currentWorld();
    if (!openBlock.has_value() || world == nullptr || openLoom_ || openAnvil_ || openGrindstone_ || openEnchantment_ ||
        openBeacon_) {
        return false;
    }
    const std::optional<std::int32_t> state = world->state().blockState(openBlock->x, openBlock->y, openBlock->z);
    const Identifier* block = state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block != nullptr && *block == world::brewingStands::block();
}

std::optional<world::brewingStands::State> ContainerMenuController::brewingState() const {
    world::LoadedWorld* world = owner_->currentWorld();
    if (!openBlock.has_value() || world == nullptr) {
        return std::nullopt;
    }
    server::world::Chunk* chunk = world->state().chunkIfLoaded(openBlock->x >> 4, openBlock->z >> 4);
    const server::world::ChunkBlockEntity* entity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(openBlock->x & 15, openBlock->y, openBlock->z & 15);
    if (entity == nullptr) {
        return std::nullopt;
    }
    return world::brewingStands::read(entity->data());
}

void ContainerMenuController::sendBrewingBars() {
    const std::optional<world::brewingStands::State> state = brewingState();
    if (!state.has_value()) {
        return;
    }
    const std::array<std::int32_t, 4> values{state->brewTime, state->fuel, state->totalBrewTime, state->totalFuel};
    for (std::size_t index = 0; index < values.size(); index++) {
        if (values[index] == openBlockData_[index]) {
            continue;
        }
        openBlockData_[index] = values[index];
        owner_->sendPlayPacket(ContainerSetDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            ContainerSetDataPacket{.containerId = openMenu.containerId(),
                                   .id = static_cast<std::int16_t>(index),
                                   .value = static_cast<std::int16_t>(values[index])}
                .write(buffer);
        });
    }
}

void ContainerMenuController::syncBrewingStand() {
    sendBrewingBars();
    PlayerSession* session = owner_->playerSession();
    const std::optional<world::brewingStands::State> state = brewingState();
    if (session == nullptr || !state.has_value()) {
        return;
    }
    for (std::size_t index = 0; index < state->items.size() && index < shownBlockSlots_.size(); index++) {
        const std::optional<std::int32_t> menuSlot =
            openMenu.menuSlotOf(player::menuSlot::Block{static_cast<std::int32_t>(index)});
        if (!menuSlot.has_value()) {
            continue;
        }
        const std::optional<server::world::loot::ItemStack>& held = state->items[index];
        const std::optional<storage::StoredItemStack> now =
            held.has_value() ? std::optional{storage::StoredItemStack{
                                   .id = held->item.toString(), .count = held->count, .components = held->components}}
                             : std::nullopt;
        if (shownBlockSlots_[index] == now) {
            continue;
        }
        shownBlockSlots_[index] = now;
        owner_->sendPlayPacket(wire::protocol::play::ContainerSetSlotPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::ContainerSetSlotPacket{.containerId = openMenu.containerId(),
                                                         .stateId = ++containerStateId_,
                                                         .slot = static_cast<std::int16_t>(*menuSlot),
                                                         .item = session->inventory.toWireStack(now)}
                .write(buffer);
        });
    }
}

std::int32_t ContainerMenuController::slotCapacity(const MenuSlot& slot) const {
    const auto* block = std::get_if<player::menuSlot::Block>(&slot);
    if (block != nullptr && block->index < world::brewingStands::kBottles && brewingStandOpen()) {
        return 1;
    }
    // parity: `ArmorSlot.getMaxStackSize`, a mount's saddle and armour.
    if (block != nullptr && block->index < 2 && mountOpen()) {
        return 1;
    }
    return PlayerInventory::kMaxStack;
}

void ContainerMenuController::quickMoveIntoBrewingStand(PlayerSession& session, const MenuSlot& from,
                                                        const storage::StoredItemStack& stack) {
    const std::optional<Identifier> item = itemOf(stack);
    if (!item.has_value()) {
        return;
    }
    std::int32_t remaining = stack.count;
    // parity: `moveItemStackTo`: stack onto matching items first, then the first empty slot only.
    const auto moveInto = [&](std::int32_t first, std::int32_t last) {
        const std::int32_t before = remaining;
        for (std::int32_t index = first; index < last && remaining > 0; index++) {
            const MenuSlot target = player::menuSlot::Block{index};
            const std::optional<storage::StoredItemStack> there = readSlot(session, target);
            if (!there.has_value() || there->id != stack.id || there->components != stack.components) {
                continue;
            }
            const std::int32_t moved = std::min(slotCapacity(target) - there->count, remaining);
            if (moved > 0) {
                writeSlot(session, target, counted(*there, there->count + moved));
                remaining -= moved;
            }
        }
        for (std::int32_t index = first; index < last && remaining > 0; index++) {
            const MenuSlot target = player::menuSlot::Block{index};
            if (readSlot(session, target).has_value() || !placeable(index, *item)) {
                continue;
            }
            const std::int32_t moved = std::min(slotCapacity(target), remaining);
            writeSlot(session, target, counted(stack, moved));
            remaining -= moved;
            break;
        }
        return remaining != before;
    };
    constexpr std::int32_t kIngredient = world::brewingStands::kIngredientSlot;
    constexpr std::int32_t kFuel = world::brewingStands::kFuelSlot;
    if (placeable(kFuel, *item)) {
        if (!moveInto(kFuel, kFuel + 1) && placeable(kIngredient, *item)) {
            moveInto(kIngredient, kIngredient + 1);
        }
    } else if (placeable(kIngredient, *item)) {
        moveInto(kIngredient, kIngredient + 1);
    } else if (placeable(0, *item)) {
        moveInto(0, world::brewingStands::kBottles);
    } else {
        return;
    }
    writeSlot(session, from, remaining <= 0 ? std::nullopt : std::optional{counted(stack, remaining)});
}

} // namespace basalt::server::runtime
