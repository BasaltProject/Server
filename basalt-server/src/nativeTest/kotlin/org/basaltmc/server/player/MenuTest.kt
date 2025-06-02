package org.basaltmc.server.player

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/**
 * The two window layouts are the same length and mean different things at almost every index, which
 * is exactly the kind of mistake that moves the wrong item instead of failing.
 */
class MenuTest {
    @Test
    fun theCraftingTableAndTheInventoryDisagreeAboutTheSameSlots() {
        val inventory = Menu.playerInventory()
        val table = Menu.craftingTable(1)

        // Slot 5 is the helmet in one window and a grid cell in the other.
        assertEquals(MenuSlot.Inventory(36), inventory.slotAt(5))
        assertEquals(MenuSlot.Grid(4), table.slotAt(5))

        // Both start with the result and both are 46 long, which is what makes the rest confusable.
        assertEquals(MenuSlot.Result, inventory.slotAt(0))
        assertEquals(MenuSlot.Result, table.slotAt(0))
        assertEquals(inventory.slotCount, table.slotCount)
    }

    @Test
    fun bothWindowsPutTheHotbarWhereTheClientExpectsIt() {
        // parity: `InventoryMenu.USE_ROW_SLOT_START` is 36, `CraftingMenu`'s is 37, because the
        // crafting table has nine grid cells where the inventory has four plus armour.
        assertEquals(MenuSlot.Inventory(PlayerInventory.HOTBAR_START), Menu.playerInventory().slotAt(36))
        assertEquals(MenuSlot.Inventory(PlayerInventory.HOTBAR_START), Menu.craftingTable(1).slotAt(37))
    }

    @Test
    fun theGridsAreTheSizeTheirWindowShows() {
        assertEquals(4, Menu.playerInventory().gridSize)
        assertEquals(9, Menu.craftingTable(1).gridSize)
    }

    @Test
    fun aSlotOutsideTheWindowResolvesToNothing() {
        assertNull(Menu.craftingTable(1).slotAt(46))
        assertNull(Menu.playerInventory().slotAt(-1))
    }
}
