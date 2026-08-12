package org.basaltmc.server.world.terrain.surface

import org.basaltmc.server.random.XoroshiroRandom
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class BadlandsClayBandsTest {
    @Test
    fun generatesAStable192BlockPaletteWithColoredBands() {
        val first = BadlandsClayBands(XoroshiroRandom(1234L))
        val second = BadlandsClayBands(XoroshiroRandom(1234L))
        val states = first.copyStateIds()

        assertEquals(192, states.size)
        assertTrue(states.toSet().size >= 6)
        assertContentEquals(states, second.copyStateIds())
    }

    @Test
    fun bandSelectionRepeatsEvery192Blocks() {
        val bands = BadlandsClayBands(XoroshiroRandom(9L))

        assertEquals(bands.stateId(-64, 0.0), bands.stateId(128, 0.0))
        assertEquals(bands.stateId(70, 0.3), bands.stateId(262, 0.3))
    }
}
