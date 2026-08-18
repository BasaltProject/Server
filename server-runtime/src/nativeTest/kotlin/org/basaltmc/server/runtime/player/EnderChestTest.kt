package org.basaltmc.server.runtime.player

import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `PlayerEnderChestContainer`, which belongs to the player and not to any chest. */
class EnderChestTest {
    /** The twenty-seven slots vanilla gives, and the same number the row in `players.db` keeps. */
    @Test
    fun itIsTwentySevenSlotsWide() {
        assertEquals(27, StoredPlayer.ENDER_CHEST_SLOTS)
    }

    /** What went in comes back out of the stored row, which is what makes it follow the player. */
    @Test
    fun itSurvivesBeingStored() {
        val slots = arrayOfNulls<StoredItemStack>(StoredPlayer.ENDER_CHEST_SLOTS)
        slots[3] = StoredItemStack("minecraft:diamond", 5)

        val stored =
            org.basaltmc.server.storage
                .StoredInventory(slots.toList())

        assertEquals(27, stored.slots.size)
        assertEquals(5, checkNotNull(stored.slots[3]).count)
        assertEquals(false, stored.isEmpty)
    }
}
