#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::player {
namespace {

/// Both the player's window and the crafting table's are this long; only what the slots mean
/// differs.
constexpr std::int32_t kSlotCount = 46;

/// The player's own storage and hotbar, tacked onto the bottom of every container window.
constexpr std::int32_t kStorageSlots = 27;
constexpr std::int32_t kPlayerSlotsInWindow = 36;

/// Where a window's own slots end and the player's begin, for the windows shaped that way.
[[nodiscard]] std::vector<std::optional<MenuSlot>> withPlayerSlots(std::vector<std::optional<MenuSlot>> slots) {
    for (std::int32_t slot = 0; slot < kPlayerSlotsInWindow; slot++) {
        slots.push_back(slot < kStorageSlots ? MenuSlot{menuSlot::Inventory{.index = slot}}
                                             : MenuSlot{menuSlot::Inventory{.index = slot - kStorageSlots +
                                                                                     PlayerInventory::kHotbarStart}});
    }
    return slots;
}

/// A window whose own slots are the block's, followed by the player's.
[[nodiscard]] std::vector<std::optional<MenuSlot>> blockWindow(std::int32_t own) {
    std::vector<std::optional<MenuSlot>> slots;
    slots.reserve(static_cast<std::size_t>(own + kPlayerSlotsInWindow));
    for (std::int32_t slot = 0; slot < own; slot++) {
        slots.emplace_back(menuSlot::Block{.index = slot});
    }
    return withPlayerSlots(std::move(slots));
}

} // namespace

Menu::Menu(std::int32_t containerId, std::int32_t gridWidth, std::int32_t gridHeight,
           std::vector<std::optional<MenuSlot>> slots)
    : containerId_(containerId), gridWidth_(gridWidth), gridHeight_(gridHeight), slots_(std::move(slots)) {}

std::optional<MenuSlot> Menu::slotAt(std::int32_t menuSlot) const {
    if (menuSlot < 0 || menuSlot >= slotCount()) {
        return std::nullopt;
    }
    return slots_[static_cast<std::size_t>(menuSlot)];
}

std::optional<std::int32_t> Menu::menuSlotOf(const MenuSlot& slot) const {
    const auto found = std::ranges::find(slots_, std::optional<MenuSlot>{slot});
    return found == slots_.end() ? std::nullopt : std::optional{static_cast<std::int32_t>(found - slots_.begin())};
}

Menu Menu::playerInventory() {
    std::vector<std::optional<MenuSlot>> slots;
    slots.reserve(kSlotCount);
    slots.emplace_back(menuSlot::Result{});
    for (std::int32_t slot = 1; slot <= 4; slot++) {
        slots.emplace_back(menuSlot::Grid{.index = slot - 1});
    }
    for (std::int32_t slot = 5; slot <= 8; slot++) {
        slots.emplace_back(menuSlot::Inventory{.index = slot - 5 + PlayerInventory::kArmourStart});
    }
    for (std::int32_t slot = 9; slot <= 35; slot++) {
        slots.emplace_back(menuSlot::Inventory{.index = slot - 9});
    }
    for (std::int32_t slot = 36; slot <= 44; slot++) {
        slots.emplace_back(menuSlot::Inventory{.index = slot - 36 + PlayerInventory::kHotbarStart});
    }
    slots.emplace_back(menuSlot::Inventory{.index = PlayerInventory::kOffhandSlot});
    return Menu{0, 2, 2, std::move(slots)};
}

Menu Menu::chest(std::int32_t containerId) { return Menu{containerId, 0, 0, blockWindow(kChestSize)}; }

Menu Menu::doubleChest(std::int32_t containerId) { return Menu{containerId, 0, 0, blockWindow(kDoubleChestSize)}; }

Menu Menu::rows(std::int32_t containerId, std::int32_t rows) {
    return Menu{containerId, 0, 0, blockWindow(rows * kColumns)};
}

Menu Menu::stonecutter(std::int32_t containerId) { return Menu{containerId, 0, 0, blockWindow(2)}; }

Menu Menu::smithing(std::int32_t containerId) { return Menu{containerId, 0, 0, blockWindow(4)}; }

Menu Menu::merchant(std::int32_t containerId) {
    std::vector<std::optional<MenuSlot>> slots;
    slots.reserve(3 + kPlayerSlotsInWindow);
    for (std::int32_t slot = 0; slot < 3; slot++) {
        slots.emplace_back(std::nullopt);
    }
    return Menu{containerId, 0, 0, withPlayerSlots(std::move(slots))};
}

Menu Menu::crafter(std::int32_t containerId) { return Menu{containerId, 0, 0, blockWindow(kCrafterSize)}; }

Menu Menu::furnace(std::int32_t containerId) { return Menu{containerId, 0, 0, blockWindow(3)}; }

Menu Menu::craftingTable(std::int32_t containerId) {
    std::vector<std::optional<MenuSlot>> slots;
    slots.reserve(kSlotCount);
    slots.emplace_back(menuSlot::Result{});
    for (std::int32_t slot = 1; slot <= 9; slot++) {
        slots.emplace_back(menuSlot::Grid{.index = slot - 1});
    }
    return Menu{containerId, 3, 3, withPlayerSlots(std::move(slots))};
}

} // namespace basalt::server::runtime::player
