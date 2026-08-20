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
import kotlin.test.assertTrue

/** parity: `RedStoneWireBlock.updatePowerStrength`, which wakes what the dust runs into. */
class RepeaterWakeTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("wake", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/repeater-wake",
            Logger("RepeaterWakeTest").apply { minLevel = LogLevel.ERROR },
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

    @Test
    fun aRepeaterAtTheEndOfTheDustWakesUpWhenTheSignalArrives() {
        // Floor, a block of redstone at x=1, dust from x=2 to x=5, and a repeater facing it at x=6.
        for (x in 0..8) world.state.setBlockState(x, 69, 3, state("stone"))
        for (x in 2..5) world.state.setBlockState(x, 70, 3, state("redstone_wire", mapOf("power" to "0")))
        world.state.setBlockState(
            6,
            70,
            3,
            state("repeater", mapOf("facing" to "east", "delay" to "2", "powered" to "false", "locked" to "false")),
        )
        world.state.setBlockState(1, 70, 3, state("redstone_block"))

        val settled = Redstone.settle(world, 1, 70, 3)

        assertTrue(settled.isNotEmpty(), "the dust should have taken the signal")
        assertEquals(
            15,
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(2, 70, 3)))["power"]?.toInt(),
        )
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        val due = assertNotNull(pending.firstOrNull { it.localX == 6 && it.y == 70 && it.localZ == 3 })
        assertEquals(4, due.delay, "two notches, two ticks each")
    }
}
