package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `RepeaterBlock.useWithoutItem` and `ComparatorBlock.useWithoutItem`. */
class DiodeClickTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("diodes", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/diode-click",
            Logger("DiodeClickTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun propertyAt(
        x: Int,
        name: String,
    ): String? = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(x, 70, 3)))[name]

    @Test
    fun clickingARepeaterMovesItsDelayOnANotch() {
        world.state.setBlockState(
            4,
            70,
            3,
            state("repeater", mapOf("facing" to "east", "delay" to "1", "powered" to "false", "locked" to "false")),
        )

        for (expected in listOf("2", "3", "4", "1")) {
            val cycled = assertNotNull(BlockInteractions.cycle(world, 4, 70, 3))
            assertNull(cycled.sound, "a repeater changes its delay in silence")
            assertEquals(expected, propertyAt(4, "delay"))
        }
    }

    @Test
    fun clickingAComparatorFlipsItsMode() {
        world.state.setBlockState(
            6,
            70,
            3,
            state("comparator", mapOf("facing" to "east", "mode" to "compare", "powered" to "false")),
        )

        val toSubtract = assertNotNull(BlockInteractions.cycle(world, 6, 70, 3))
        assertEquals("subtract", propertyAt(6, "mode"))
        assertEquals("block.comparator.click", toSubtract.sound)
        assertTrue(toSubtract.pitch > 0.5f, "subtract clicks higher than compare")

        val backToCompare = assertNotNull(BlockInteractions.cycle(world, 6, 70, 3))
        assertEquals("compare", propertyAt(6, "mode"))
        assertEquals(0.5f, backToCompare.pitch)
    }

    @Test
    fun aHandDoesNothingToADustLine() {
        world.state.setBlockState(8, 70, 3, state("redstone_wire", mapOf("power" to "0")))
        assertNull(BlockInteractions.cycle(world, 8, 70, 3))
    }
}
