package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.player.Menu
import org.basaltmc.server.runtime.player.MenuSlot
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** parity: `AbstractFurnaceMenu`'s slot order, which is what a click indexes. */
class FurnaceMenuTest {
    private val menu = Menu.furnace(containerId = 3)

    @Test
    fun theFirstThreeSlotsAreTheBlocksOwn() {
        assertEquals(MenuSlot.Block(Furnaces.INPUT_SLOT), menu.slotAt(0))
        assertEquals(MenuSlot.Block(Furnaces.FUEL_SLOT), menu.slotAt(1))
        assertEquals(MenuSlot.Block(Furnaces.RESULT_SLOT), menu.slotAt(2))
    }

    @Test
    fun thenTheStorageAndTheHotbar() {
        assertEquals(MenuSlot.Inventory(0), menu.slotAt(3), "the first storage slot follows the block's three")
        assertEquals(MenuSlot.Inventory(26), menu.slotAt(29), "and the last")
        assertEquals(MenuSlot.Inventory(27), menu.slotAt(30), "then the hotbar")
        assertEquals(MenuSlot.Inventory(35), menu.slotAt(38))
        assertNull(menu.slotAt(39), "a cooker's window is thirty-nine slots long")
        assertEquals(39, menu.slotCount)
    }
}
