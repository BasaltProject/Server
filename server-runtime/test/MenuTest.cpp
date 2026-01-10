#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/player/Menu.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>

#include <doctest.h>

#include <cstdint>

using basalt::server::runtime::player::Menu;
using basalt::server::runtime::player::MenuSlot;
using basalt::server::runtime::player::PlayerInventory;
namespace menuSlot = basalt::server::runtime::player::menuSlot;

namespace {

/// The player's storage and hotbar sit at the bottom of every container window, in that order.
void checkPlayerSlotsFrom(const Menu& menu, std::int32_t own) {
    for (std::int32_t slot = 0; slot < 27; slot++) {
        CHECK(menu.slotAt(own + slot) == MenuSlot{menuSlot::Inventory{.index = slot}});
    }
    for (std::int32_t slot = 0; slot < 9; slot++) {
        CHECK(menu.slotAt(own + 27 + slot) ==
              MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kHotbarStart + slot}});
    }
    CHECK(menu.slotCount() == own + 36);
}

} // namespace

TEST_CASE("the player's own window is the result, a small grid, the armour and the rest") {
    const Menu menu = Menu::playerInventory();

    CHECK(menu.containerId() == 0);
    CHECK(menu.slotCount() == 46);
    CHECK(menu.gridWidth() == 2);
    CHECK(menu.gridHeight() == 2);
    CHECK(menu.gridSize() == 4);

    CHECK(menu.slotAt(0) == MenuSlot{menuSlot::Result{}});
    CHECK(menu.slotAt(1) == MenuSlot{menuSlot::Grid{.index = 0}});
    CHECK(menu.slotAt(4) == MenuSlot{menuSlot::Grid{.index = 3}});
    // parity: `InventoryMenu` puts the armour where a crafting table puts the top row of its grid.
    CHECK(menu.slotAt(5) == MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kArmourStart}});
    CHECK(menu.slotAt(8) == MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kArmourStart + 3}});
    CHECK(menu.slotAt(9) == MenuSlot{menuSlot::Inventory{.index = 0}});
    CHECK(menu.slotAt(35) == MenuSlot{menuSlot::Inventory{.index = 26}});
    CHECK(menu.slotAt(36) == MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kHotbarStart}});
    CHECK(menu.slotAt(44) == MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kHotbarStart + 8}});
    CHECK(menu.slotAt(45) == MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kOffhandSlot}});
}

TEST_CASE("the player's window and the inventory's own numbering agree with each other") {
    const Menu menu = Menu::playerInventory();

    for (std::int32_t storage = 0; storage < 41; storage++) {
        CHECK(menu.menuSlotOf(MenuSlot{menuSlot::Inventory{.index = storage}}) == PlayerInventory::menuSlotOf(storage));
    }
}

TEST_CASE("a slot outside a window refers to nothing") {
    const Menu menu = Menu::playerInventory();

    CHECK_FALSE(menu.slotAt(-1).has_value());
    CHECK_FALSE(menu.slotAt(46).has_value());
    CHECK_FALSE(menu.menuSlotOf(MenuSlot{menuSlot::Block{.index = 0}}).has_value());
}

TEST_CASE("a crafting table is the same length as the player's window and means something else") {
    const Menu menu = Menu::craftingTable(3);

    CHECK(menu.containerId() == 3);
    CHECK(menu.slotCount() == 46);
    CHECK(menu.gridSize() == 9);
    CHECK(menu.slotAt(0) == MenuSlot{menuSlot::Result{}});
    CHECK(menu.slotAt(1) == MenuSlot{menuSlot::Grid{.index = 0}});
    CHECK(menu.slotAt(9) == MenuSlot{menuSlot::Grid{.index = 8}});
    // parity: where the player's window keeps the armour, this one is already into the grid.
    CHECK(menu.slotAt(5) == MenuSlot{menuSlot::Grid{.index = 4}});
    checkPlayerSlotsFrom(menu, 10);
}

TEST_CASE("a chest is its own rows and then the player's") {
    const Menu menu = Menu::chest(1);

    CHECK(menu.slotCount() == 63);
    CHECK(menu.slotAt(0) == MenuSlot{menuSlot::Block{.index = 0}});
    CHECK(menu.slotAt(26) == MenuSlot{menuSlot::Block{.index = 26}});
    checkPlayerSlotsFrom(menu, Menu::kChestSize);
}

TEST_CASE("a double chest is twice the rows and the same player slots after them") {
    const Menu menu = Menu::doubleChest(2);

    CHECK(menu.slotCount() == 90);
    CHECK(menu.slotAt(53) == MenuSlot{menuSlot::Block{.index = 53}});
    checkPlayerSlotsFrom(menu, Menu::kDoubleChestSize);
}

TEST_CASE("a window of rows is as long as the rows it was asked for") {
    for (std::int32_t rows = 1; rows <= 6; rows++) {
        const Menu menu = Menu::rows(7, rows);
        CHECK(menu.slotCount() == (rows * Menu::kColumns) + 36);
        CHECK(menu.slotAt(0) == MenuSlot{menuSlot::Block{.index = 0}});
        checkPlayerSlotsFrom(menu, rows * Menu::kColumns);
    }
}

TEST_CASE("a furnace has three slots of its own") {
    const Menu menu = Menu::furnace(4);

    CHECK(menu.slotCount() == 39);
    CHECK(menu.slotAt(0) == MenuSlot{menuSlot::Block{.index = 0}});
    CHECK(menu.slotAt(2) == MenuSlot{menuSlot::Block{.index = 2}});
    checkPlayerSlotsFrom(menu, 3);
}

TEST_CASE("a stonecutter has one slot in and one out") {
    const Menu menu = Menu::stonecutter(5);

    CHECK(menu.slotCount() == 38);
    CHECK(menu.slotAt(Menu::kStonecutterInput) == MenuSlot{menuSlot::Block{.index = 0}});
    CHECK(menu.slotAt(Menu::kStonecutterResult) == MenuSlot{menuSlot::Block{.index = 1}});
    checkPlayerSlotsFrom(menu, 2);
}

TEST_CASE("a smithing table has its four") {
    const Menu menu = Menu::smithing(6);

    CHECK(menu.slotCount() == 40);
    CHECK(menu.slotAt(Menu::kSmithingTemplate) == MenuSlot{menuSlot::Block{.index = 0}});
    CHECK(menu.slotAt(Menu::kSmithingResult) == MenuSlot{menuSlot::Block{.index = 3}});
    checkPlayerSlotsFrom(menu, 4);
}

TEST_CASE("a crafter has nine and no result to take from") {
    const Menu menu = Menu::crafter(8);

    CHECK(menu.slotCount() == 45);
    CHECK(menu.slotAt(8) == MenuSlot{menuSlot::Block{.index = 8}});
    CHECK_FALSE(menu.menuSlotOf(MenuSlot{menuSlot::Result{}}).has_value());
    checkPlayerSlotsFrom(menu, Menu::kCrafterSize);
}

TEST_CASE("a villager's three trading slots are nobody's container") {
    const Menu menu = Menu::merchant(9);

    CHECK(menu.slotCount() == 39);
    CHECK_FALSE(menu.slotAt(0).has_value());
    CHECK_FALSE(menu.slotAt(2).has_value());
    checkPlayerSlotsFrom(menu, 3);
}

TEST_CASE("a window says where a slot of it is drawn") {
    const Menu menu = Menu::chest(1);

    CHECK(menu.menuSlotOf(MenuSlot{menuSlot::Block{.index = 5}}) == 5);
    CHECK(menu.menuSlotOf(MenuSlot{menuSlot::Inventory{.index = 0}}) == Menu::kChestSize);
    CHECK(menu.menuSlotOf(MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kHotbarStart}}) ==
          Menu::kChestSize + 27);
    // A chest shows no armour, so it has nowhere to draw it.
    CHECK_FALSE(menu.menuSlotOf(MenuSlot{menuSlot::Inventory{.index = PlayerInventory::kArmourStart}}).has_value());
}
