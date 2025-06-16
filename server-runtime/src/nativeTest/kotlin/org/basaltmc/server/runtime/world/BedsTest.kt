package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class BedsTest {
    private fun bed(properties: Map<String, String>): Int =
        checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("red_bed"), properties))

    @Test
    fun aBedIsToldApartFromEverythingElse() {
        assertTrue(Beds.isBed(bed(emptyMap())))
        assertFalse(Beds.isBed(Blocks.STONE.stateId))
    }

    @Test
    fun theOccupiedFlagGoesOnAndOff() {
        val empty = bed(mapOf("occupied" to "false", "part" to "head", "facing" to "north"))
        val taken = Beds.withOccupied(empty, occupied = true)
        assertTrue(Beds.isOccupied(taken))
        assertFalse(Beds.isOccupied(Beds.withOccupied(taken, occupied = false)))
        // Nothing else about the block moves with it.
        assertEquals("north", Beds.facingOf(taken))
        assertTrue(Beds.isHead(taken))
    }

    /** The pillow is one block along the way the bed faces, from the foot. */
    @Test
    fun theHeadIsOneStepAlongTheFacing() {
        assertEquals(listOf(0, 0, -1), Beds.headOffset(bed(mapOf("facing" to "north"))).toList())
        assertEquals(listOf(1, 0, 0), Beds.headOffset(bed(mapOf("facing" to "east"))).toList())
    }

    /** parity: `Level.isNight`, the window a bed will take anyone in. */
    @Test
    fun aBedOnlyWorksAtNight() {
        assertFalse(Beds.isNight(0))
        assertFalse(Beds.isNight(6000))
        assertTrue(Beds.isNight(13000))
        assertTrue(Beds.isNight(23000))
        assertFalse(Beds.isNight(23999))
        // And on the second day just the same.
        assertTrue(Beds.isNight(24000 + 13000))
    }

    @Test
    fun morningIsTheStartOfTheNextDay() {
        assertEquals(24000, Beds.nextMorning(13000))
        assertEquals(48000, Beds.nextMorning(24000 + 18000))
    }

    /** parity: a bed is only a bed in the Overworld; anywhere else it goes off. */
    @Test
    fun aBedOnlyWorksInTheOverworld() {
        assertFalse(Beds.explodesIn(WorldDimension.OVERWORLD))
        assertTrue(Beds.explodesIn(WorldDimension.NETHER))
        assertTrue(Beds.explodesIn(WorldDimension.END))
    }
}
