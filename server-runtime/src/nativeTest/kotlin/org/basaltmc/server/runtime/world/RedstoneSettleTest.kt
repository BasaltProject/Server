package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
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
import kotlin.test.assertTrue

/** parity: `Level.updateNeighborsAt` into `RedStoneWireBlock.neighborChanged`. */
class RedstoneSettleTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("settle", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/redstone-settle",
            Logger("RedstoneSettleTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun world() = world

    private fun dust(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ) {
        world.state.setBlockState(x, y, z, checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("redstone_wire"))))
    }

    private fun powerAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val state = world.state.getBlockState(x, y, z) ?: return -1
        return BlockStateDefinitions.propertiesOf(state)["power"]?.toIntOrNull() ?: -1
    }

    @Test
    fun aSourcePlacedBesideExistingDustLightsIt() {
        val world = world()
        for (x in 0..5) world.state.setBlockState(x, 63, 0, Blocks.STONE.stateId)
        for (x in 1..5) dust(world, x, 64, 0)
        assertEquals(0, powerAt(world, 1, 64, 0))

        val redstoneBlock = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("redstone_block")))
        world.state.setBlockState(0, 64, 0, redstoneBlock)
        Redstone.settle(world, 0, 64, 0)

        assertEquals(15, powerAt(world, 1, 64, 0), "the dust beside the block")
        assertEquals(14, powerAt(world, 2, 64, 0))
        assertEquals(11, powerAt(world, 5, 64, 0))
    }

    @Test
    fun takingTheSourceAwayPutsTheDustOutAgain() {
        val world = world()
        for (x in 0..5) world.state.setBlockState(x, 63, 0, Blocks.STONE.stateId)
        for (x in 1..5) dust(world, x, 64, 0)
        val redstoneBlock = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("redstone_block")))
        world.state.setBlockState(0, 64, 0, redstoneBlock)
        Redstone.settle(world, 0, 64, 0)
        assertTrue(powerAt(world, 1, 64, 0) > 0)

        world.state.setBlockState(0, 64, 0, Blocks.AIR.stateId)
        Redstone.settle(world, 0, 64, 0)
        assertEquals(0, powerAt(world, 1, 64, 0))
        assertEquals(0, powerAt(world, 5, 64, 0))
    }

    @Test
    fun dustBesideTheBlockALeverSitsOnLightsUp() {
        val world = world()
        // A block with a lever on top, and dust on the ground beside it: the dust is two steps from
        // the lever, which is the shape of nearly every real circuit.
        world.state.setBlockState(0, 63, 0, Blocks.STONE.stateId)
        world.state.setBlockState(1, 63, 0, Blocks.STONE.stateId)
        dust(world, 1, 64, 0)
        val lever =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("lever"),
                    mapOf("face" to "floor", "facing" to "north", "powered" to "true"),
                ),
            )
        world.state.setBlockState(0, 64, 0, lever)
        Redstone.settle(world, 0, 64, 0)

        assertEquals(15, powerAt(world, 1, 64, 0))
    }
}
