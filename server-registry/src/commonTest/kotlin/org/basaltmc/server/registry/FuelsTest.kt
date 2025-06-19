package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: the numbers `FuelValues.vanillaBurnTimes` builds, over a base unit of 200 ticks. */
class FuelsTest {
    @Test
    fun theNumbersEveryoneKnows() {
        assertEquals(20000, Fuels.burnTicks(Identifier.vanilla("lava_bucket")), "a hundred base units")
        assertEquals(16000, Fuels.burnTicks(Identifier.vanilla("coal_block")))
        assertEquals(1600, Fuels.burnTicks(Identifier.vanilla("coal")), "eight items out of one coal")
        assertEquals(1600, Fuels.burnTicks(Identifier.vanilla("charcoal")))
        assertEquals(300, Fuels.burnTicks(Identifier.vanilla("oak_planks")), "a plank and a half")
        assertEquals(100, Fuels.burnTicks(Identifier.vanilla("stick")), "half an item")
    }

    /** parity: the `.remove(#non_flammable_wood)` at the end of the table. */
    @Test
    fun crimsonAndWarpedWoodDoNotBurn() {
        assertFalse(Fuels.isFuel(Identifier.vanilla("crimson_planks")))
        assertFalse(Fuels.isFuel(Identifier.vanilla("warped_planks")))
        assertFalse(Fuels.isFuel(Identifier.vanilla("crimson_door")))
    }

    @Test
    fun stoneIsNotFuel() {
        assertFalse(Fuels.isFuel(Identifier.vanilla("stone")))
        assertEquals(0, Fuels.burnTicks(Identifier.vanilla("iron_ingot")))
    }

    @Test
    fun theTableIsTheWholeOne() {
        assertTrue(Fuels.all.size > 200, "only ${Fuels.all.size} fuels came out of the jar")
    }
}
