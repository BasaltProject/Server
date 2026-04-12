#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/CombatHandler.hpp>
#include <basalt/server/runtime/ContainerMenuController.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/InventoryPackets.hpp>
#include <basalt/wire/protocol/play/VehiclePackets.hpp>

#include <algorithm>
#include <expected>
#include <vector>

namespace basalt::server::runtime {
namespace {

using player::Menu;
using player::MenuSlot;
using player::PlayerInventory;
using player::PlayerSession;
namespace horses = entity::horses;

[[nodiscard]] storage::StoredItemStack storedOf(const server::world::loot::ItemStack& stack) {
    return storage::StoredItemStack{.id = stack.item.toString(), .count = stack.count, .components = stack.components};
}

[[nodiscard]] std::optional<server::world::loot::ItemStack>
lootOf(const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(stack->id);
    return id.has_value() ? std::optional{server::world::loot::ItemStack{
                                .item = *id, .count = stack->count, .components = stack->components}}
                          : std::nullopt;
}

[[nodiscard]] storage::StoredItemStack counted(const storage::StoredItemStack& stack, std::int32_t count) {
    storage::StoredItemStack copy = stack;
    copy.count = count;
    return copy;
}

[[nodiscard]] std::optional<entity::mobEquipment::Slot> equipmentSlotAt(std::int32_t index) {
    if (index == 0) {
        return entity::mobEquipment::Slot::Saddle;
    }
    if (index == 1) {
        return entity::mobEquipment::Slot::Body;
    }
    return std::nullopt;
}

} // namespace

void ContainerMenuController::openMountMenu(PlayerSession& session, entity::ServerEntity& mount) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    returnCraftingGrid(session);
    const std::int32_t columns = horses::inventoryColumns(mount);
    openMenu = Menu::mount(allocateContainerId(), columns);
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_ = mount.entityId();
    openMount_ = true;
    openBlockSize = horses::kEquipmentSlots + (columns * horses::kInventoryRows);
    openBlockData_ = {-1, -1, -1, -1};
    if (!mount.sourceNbt.has_value()) {
        mount.sourceNbt = wire::nbt::NbtCompound{};
    }
    owner_->sendCompressed(wire::protocol::play::MountScreenOpenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::MountScreenOpenPacket{
            .containerId = openMenu.containerId(), .inventoryColumns = columns, .entityId = mount.entityId()}
            .write(buffer);
    });
    sendInventory();
}

bool ContainerMenuController::mountAccepts(std::int32_t index, const storage::StoredItemStack& stack) const {
    const std::optional<entity::mobEquipment::Slot> equipment = equipmentSlotAt(index);
    if (!equipment.has_value()) {
        return true;
    }
    world::LoadedWorld* world = owner_->currentWorld();
    const entity::ServerEntity* mount = world == nullptr ? nullptr : world->state().entity(*openEntity_);
    const auto item = wire::common::Identifier::parse(stack.id);
    if (mount == nullptr || !item.has_value()) {
        return false;
    }
    const horses::Wearable wearable = horses::wearableFor(*mount, *item);
    return *equipment == entity::mobEquipment::Slot::Saddle ? wearable == horses::Wearable::Saddle
                                                            : wearable == horses::Wearable::Body;
}

void ContainerMenuController::quickMoveMount(PlayerSession& session, const MenuSlot& slot,
                                             const storage::StoredItemStack& stack) {
    std::int32_t remaining = stack.count;
    // parity: `moveItemStackTo`: onto matching stacks first, then into the first empty slot.
    const auto moveInto = [&](const std::vector<MenuSlot>& targets, std::int32_t perSlot) {
        const std::int32_t before = remaining;
        for (const MenuSlot& target : targets) {
            const std::optional<storage::StoredItemStack> there = readSlot(session, target);
            if (remaining <= 0 || !there.has_value() || there->id != stack.id ||
                there->components != stack.components) {
                continue;
            }
            const std::int32_t moved = std::min(remaining, perSlot - there->count);
            if (moved > 0) {
                writeSlot(session, target, counted(*there, there->count + moved));
                remaining -= moved;
            }
        }
        for (const MenuSlot& target : targets) {
            if (remaining <= 0) {
                break;
            }
            if (!readSlot(session, target).has_value()) {
                const std::int32_t moved = std::min(remaining, perSlot);
                writeSlot(session, target, counted(stack, moved));
                remaining -= moved;
                break;
            }
        }
        return remaining != before;
    };
    const auto players = [](std::int32_t from, std::int32_t to, bool reverse) {
        std::vector<MenuSlot> out;
        for (std::int32_t index = from; index < to; ++index) {
            out.emplace_back(player::menuSlot::Inventory{index});
        }
        if (reverse) {
            std::ranges::reverse(out);
        }
        return out;
    };
    const auto* fromInventory = std::get_if<player::menuSlot::Inventory>(&slot);
    const std::int32_t chest = openBlockSize - horses::kEquipmentSlots;
    if (fromInventory == nullptr) {
        // parity: out of the mount into the player's storage then hotbar, from the far end.
        std::vector<MenuSlot> targets = players(PlayerInventory::kHotbarStart, PlayerInventory::kMainSlots, true);
        const std::vector<MenuSlot> storageRows = players(0, PlayerInventory::kHotbarStart, true);
        targets.insert(targets.end(), storageRows.begin(), storageRows.end());
        if (!moveInto(targets, PlayerInventory::kMaxStack)) {
            return;
        }
    } else if (mountAccepts(1, stack) && !readSlot(session, player::menuSlot::Block{1}).has_value()) {
        static_cast<void>(moveInto({player::menuSlot::Block{1}}, 1));
    } else if (mountAccepts(0, stack) && !readSlot(session, player::menuSlot::Block{0}).has_value()) {
        static_cast<void>(moveInto({player::menuSlot::Block{0}}, 1));
    } else {
        std::vector<MenuSlot> chestSlots;
        for (std::int32_t index = 0; index < chest; ++index) {
            chestSlots.emplace_back(player::menuSlot::Block{horses::kEquipmentSlots + index});
        }
        if (chest == 0 || !moveInto(chestSlots, PlayerInventory::kMaxStack)) {
            // parity: nothing on the mount takes it, so it crosses between storage and hotbar.
            if (fromInventory->index >= PlayerInventory::kHotbarStart) {
                static_cast<void>(
                    moveInto(players(0, PlayerInventory::kHotbarStart, false), PlayerInventory::kMaxStack));
            } else {
                static_cast<void>(moveInto(players(PlayerInventory::kHotbarStart, PlayerInventory::kMainSlots, false),
                                           PlayerInventory::kMaxStack));
            }
        }
    }
    writeSlot(session, slot, remaining <= 0 ? std::nullopt : std::optional{counted(stack, remaining)});
}

void ContainerMenuController::openEntityChest(PlayerSession& session, entity::ServerEntity& vehicle) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    returnCraftingGrid(session);
    openMenu = Menu::chest(allocateContainerId());
    openBlock.reset();
    openBlockSecond.reset();
    openEnderChest_ = false;
    openEntity_ = vehicle.entityId();
    openMount_ = false;
    openBlockSize = Menu::kChestSize;
    openBlockData_ = {-1, -1, -1, -1};
    if (!vehicle.sourceNbt.has_value()) {
        vehicle.sourceNbt = wire::nbt::NbtCompound{};
    }
    owner_->sendCompressed(wire::protocol::play::OpenScreenPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::OpenScreenPacket{.containerId = openMenu.containerId(),
                                               .menuType = wire::protocol::play::menuType::kGeneric9x3,
                                               .title = entity::entityCombat::displayName(vehicle)}
            .write(buffer);
    });
    sendInventory();
    // parity: `GameEvent.CONTAINER_OPEN` from the player at the vehicle.
    world::vibrations::emit(world->state(), "container_open", vehicle.x, vehicle.y, vehicle.z, world->gameTime(),
                            {.entity = session.uuid()});
}

std::optional<storage::StoredItemStack> ContainerMenuController::entityContainerSlot(std::int32_t index) const {
    world::LoadedWorld* world = owner_->currentWorld();
    const entity::ServerEntity* vehicle = world == nullptr ? nullptr : world->state().entity(*openEntity_);
    if (vehicle != nullptr && openMount_) {
        if (const std::optional<entity::mobEquipment::Slot> equipment = equipmentSlotAt(index); equipment.has_value()) {
            const std::optional<server::world::loot::ItemStack> worn = entity::mobEquipment::get(*vehicle, *equipment);
            return worn.has_value() ? std::optional{storedOf(*worn)} : std::nullopt;
        }
        index -= horses::kEquipmentSlots;
    }
    const std::int32_t size = openMount_ ? openBlockSize - horses::kEquipmentSlots : Menu::kChestSize;
    if (vehicle == nullptr || !vehicle->sourceNbt.has_value() || index < 0 || index >= size) {
        return std::nullopt;
    }
    const world::blockInventory::Slots slots = world::blockInventory::read(*vehicle->sourceNbt, size);
    const std::optional<server::world::loot::ItemStack>& slot = slots[static_cast<std::size_t>(index)];
    return slot.has_value() ? std::optional{storedOf(*slot)} : std::nullopt;
}

void ContainerMenuController::writeEntityContainerSlot(std::int32_t index,
                                                       const std::optional<storage::StoredItemStack>& stack) {
    world::LoadedWorld* world = owner_->currentWorld();
    entity::ServerEntity* vehicle = world == nullptr ? nullptr : world->state().entity(*openEntity_);
    if (vehicle != nullptr && openMount_) {
        if (const std::optional<entity::mobEquipment::Slot> equipment = equipmentSlotAt(index); equipment.has_value()) {
            owner_->combat().equipMount(*vehicle, *equipment, lootOf(stack));
            return;
        }
        index -= horses::kEquipmentSlots;
    }
    const std::int32_t size = openMount_ ? openBlockSize - horses::kEquipmentSlots : Menu::kChestSize;
    if (vehicle == nullptr || index < 0 || index >= size) {
        return;
    }
    if (!vehicle->sourceNbt.has_value()) {
        vehicle->sourceNbt = wire::nbt::NbtCompound{};
    }
    world::blockInventory::Slots slots = world::blockInventory::read(*vehicle->sourceNbt, size);
    slots[static_cast<std::size_t>(index)] = lootOf(stack);
    world::blockInventory::write(*vehicle->sourceNbt, slots);
}

} // namespace basalt::server::runtime
