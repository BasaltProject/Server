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

/** parity: `Level.updateNeighborsAt`, which is what a lamp beside dust or a repeater waits for. */
class LampLineTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("lamps", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/lamp-line",
            Logger("LampLineTest").apply { minLevel = LogLevel.ERROR },
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

    private fun lit(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(x, y, z)))["lit"]

    @Test
    fun aRepeaterOnItsOwnTurnLightsWhatItFeeds() {
        for (x in 0..8) world.state.setBlockState(x, 69, 5, state("stone"))
        world.state.setBlockState(2, 70, 5, state("redstone_block"))
        world.state.setBlockState(
            3,
            70,
            5,
            state("repeater", mapOf("facing" to "west", "delay" to "1", "powered" to "false", "locked" to "false")),
        )
        world.state.setBlockState(4, 70, 5, state("redstone_lamp", mapOf("lit" to "false")))

        val moved = checkNotNull(RedstoneComponents.tick(world, 3, 70, 5))
        assertEquals("true", BlockStateDefinitions.propertiesOf(moved.stateId)["powered"])
        val reacted = RedstoneReactors.update(world, moved.x, moved.y, moved.z)

        assertEquals(1, reacted.size, "the lamp beside it should have reacted")
        assertEquals("true", lit(4, 70, 5))
    }

    @Test
    fun aLampAtTheEndOfTheDustLightsWhenTheSignalArrives() {
        for (x in 0..12) world.state.setBlockState(x, 69, 7, state("stone"))
        for (x in 2..6) world.state.setBlockState(x, 70, 7, state("redstone_wire", mapOf("power" to "0")))
        world.state.setBlockState(7, 70, 7, state("redstone_lamp", mapOf("lit" to "false")))
        world.state.setBlockState(1, 70, 7, state("redstone_block"))

        var reacted = 0
        for (change in Redstone.settle(world, 1, 70, 7)) {
            reacted += RedstoneReactors.update(world, change.x, change.y, change.z).size
        }

        assertEquals("true", lit(7, 70, 7), "the lamp is at the end of the line, not beside the source")
        assertEquals(1, reacted)
    }
}
