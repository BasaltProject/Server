package org.basaltmc.server.player

import org.basaltmc.storage.StoredInventory
import org.basaltmc.storage.StoredItemStack
import kotlin.test.Test
import kotlin.test.assertEquals
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

        // Nothing is dropped on the floor because item entities do not exist yet, so the caller has
        // to be told rather than left assuming it all landed.
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
}
