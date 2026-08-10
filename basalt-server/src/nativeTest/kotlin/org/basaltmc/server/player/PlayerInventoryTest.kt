package org.basaltmc.server.player

import org.basaltmc.storage.StoredInventory
import org.basaltmc.storage.StoredItemStack
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class PlayerInventoryTest {
    private fun stack(
        id: String = "minecraft:stone",
        count: Int = 1,
    ) = StoredItemStack(id, count)

    @Test
    fun somethingPickedUpGoesToTheHotbarFirst() {
        val inventory = PlayerInventory()

        inventory.add(stack(count = 1))

        // Hotbar first is what vanilla does and what a player expects: a new item is to hand.
        assertEquals(1, inventory[PlayerInventory.HOTBAR_START]?.count)
    }

    @Test
    fun aMatchingStackIsToppedUpBeforeAnEmptySlotIsUsed() {
        val inventory = PlayerInventory()
        inventory[PlayerInventory.HOTBAR_START] = stack(count = 60)

        inventory.add(stack(count = 10))

        assertEquals(64, inventory[PlayerInventory.HOTBAR_START]?.count)
        // The six that did not fit start a second stack rather than being lost.
        assertEquals(6, inventory[PlayerInventory.HOTBAR_START + 1]?.count)
    }

    @Test
    fun aDifferentItemNeverStacksOntoAnother() {
        val inventory = PlayerInventory()
        inventory[PlayerInventory.HOTBAR_START] = stack(id = "minecraft:dirt", count = 1)

        inventory.add(stack(id = "minecraft:stone", count = 1))

        assertEquals("minecraft:dirt", inventory[PlayerInventory.HOTBAR_START]?.id)
        assertEquals("minecraft:stone", inventory[PlayerInventory.HOTBAR_START + 1]?.id)
    }

    @Test
    fun whatDoesNotFitIsReportedRatherThanVanishing() {
        val inventory = PlayerInventory()
        for (slot in 0 until StoredInventory.PLAYER_SLOTS) inventory[slot] = stack(count = 64)

        // The container itself never drops anything on the floor: that is the caller's job (see
        // ClientSession.tryPickupNearbyItems), so it has to be told rather than left assuming it all landed.
        assertEquals(5, inventory.add(stack(count = 5)))
    }

    @Test
    fun theHeldSlotStaysInsideTheHotbar() {
        val inventory = PlayerInventory()

        inventory.heldSlot = 99
        assertEquals(PlayerInventory.HOTBAR_SIZE - 1, inventory.heldSlot)
        inventory.heldSlot = -3
        assertEquals(0, inventory.heldSlot)
    }

    @Test
    fun theHeldItemIsWhateverIsInThatHotbarSlot() {
        val inventory = PlayerInventory()
        inventory[PlayerInventory.HOTBAR_START + 2] = stack(id = "minecraft:diamond", count = 1)

        inventory.heldSlot = 2

        assertEquals("minecraft:diamond", inventory.heldItem?.id)
    }

    @Test
    fun anInventorySurvivesBeingSavedAndPutBack() {
        val inventory = PlayerInventory()
        inventory[0] = stack(id = "minecraft:oak_log", count = 12)
        inventory[40] = stack(id = "minecraft:shield", count = 1)

        val restored = PlayerInventory().apply { restore(inventory.snapshot()) }

        assertEquals("minecraft:oak_log", restored[0]?.id)
        assertEquals(12, restored[0]?.count)
        assertEquals("minecraft:shield", restored[40]?.id)
    }

    @Test
    fun anItemThisBuildDoesNotKnowIsLeftOutRatherThanSentAsSomethingElse() {
        val inventory = PlayerInventory()
        inventory[0] = stack(id = "someplugin:mystery", count = 1)
        inventory[1] = stack(id = "minecraft:stone", count = 1)

        // A wrong id is a wrong item in someone's hand, which is worse than an empty slot.
        assertNull(inventory.toWire(0))
        assertTrue(inventory.toWire(1) != null)
    }

    @Test
    fun theWireFormIsTheFortySixSlotMenuNotTheFortyOneStored() {
        val inventory = PlayerInventory()
        inventory[PlayerInventory.HOTBAR_START] = stack(id = "minecraft:dirt", count = 1)

        val wire = inventory.toWire()

        // A real client drops a window packet whose slot count disagrees with its own menu, which
        // is why every server-side inventory change used to be invisible.
        assertEquals(PlayerInventory.MENU_SLOTS, wire.size)
        // The first hotbar slot is menu slot 36 (`InventoryMenu.USE_ROW_SLOT_START`), not 27.
        assertNotNull(wire[36])
        assertNull(wire[27])
        // The crafting result and the 2x2 grid are modelled by nothing here, so they go out empty.
        for (craftingSlot in 0..4) assertNull(wire[craftingSlot])
    }

    @Test
    fun menuAndStorageSlotsRoundTripBothWays() {
        // Every menu slot this build models must map back to the storage slot it came from, or a
        // creative-menu edit lands somewhere other than where the player put it.
        for (menuSlot in 0 until PlayerInventory.MENU_SLOTS) {
            val storageSlot = PlayerInventory.storageSlotOf(menuSlot) ?: continue
            assertEquals(menuSlot, PlayerInventory.menuSlotOf(storageSlot))
        }
        assertEquals(36, PlayerInventory.menuSlotOf(PlayerInventory.HOTBAR_START))
        assertEquals(45, PlayerInventory.menuSlotOf(40))
        // The crafting result and grid map to nothing this build stores.
        for (craftingSlot in 0..4) assertNull(PlayerInventory.storageSlotOf(craftingSlot))
    }

    @Test
    fun aPickedItemPrefersAnEmptyHotbarSlotStartingFromTheHeldOne() {
        val inventory = PlayerInventory()
        inventory.heldSlot = 3
        // The held slot is taken, so the search wraps forward to the next empty one.
        inventory[PlayerInventory.HOTBAR_START + 3] = stack(id = "minecraft:stone")

        assertEquals(4, inventory.suitableHotbarSlot())
    }

    @Test
    fun aPickedItemWrapsAroundTheHotbarRatherThanGivingUp() {
        val inventory = PlayerInventory()
        inventory.heldSlot = 8
        for (slot in 1 until PlayerInventory.HOTBAR_SIZE) {
            inventory[PlayerInventory.HOTBAR_START + slot] = stack(id = "minecraft:stone")
        }

        // Only slot 0 is free, and it is "before" the held slot: the scan wraps to reach it.
        assertEquals(0, inventory.suitableHotbarSlot())
    }

    @Test
    fun aFullHotbarFallsBackToTheHeldSlot() {
        val inventory = PlayerInventory()
        inventory.heldSlot = 5
        for (slot in 0 until PlayerInventory.HOTBAR_SIZE) {
            inventory[PlayerInventory.HOTBAR_START + slot] = stack(id = "minecraft:stone")
        }

        // Nothing is free, so vanilla overwrites the slot in hand rather than picking at random.
        assertEquals(5, inventory.suitableHotbarSlot())
    }

    @Test
    fun indexOfFindsTheFirstSlotCarryingAnItem() {
        val inventory = PlayerInventory()
        inventory[5] = stack(id = "minecraft:diamond", count = 1)
        inventory[12] = stack(id = "minecraft:diamond", count = 1)

        assertEquals(5, inventory.indexOf("minecraft:diamond"))
        assertNull(inventory.indexOf("minecraft:emerald"))
    }

    @Test
    fun swapExchangesTwoSlots() {
        val inventory = PlayerInventory()
        inventory[0] = stack(id = "minecraft:diamond", count = 1)
        inventory[1] = stack(id = "minecraft:stone", count = 3)

        inventory.swap(0, 1)

        assertEquals("minecraft:stone", inventory[0]?.id)
        assertEquals("minecraft:diamond", inventory[1]?.id)
    }
}
