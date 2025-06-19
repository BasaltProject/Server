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
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class FallingBlocksTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("falling", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/falling",
            Logger("FallingTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private var nextEntityId = 1

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(name: String): Int =
        checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), emptyMap()))

    @Test
    fun sandFallsAndGravelDoesAndStoneDoesNot() {
        assertTrue(FallingBlocks.falls(state("sand")))
        assertTrue(FallingBlocks.falls(state("gravel")))
        assertTrue(FallingBlocks.falls(state("anvil")))
        assertFalse(FallingBlocks.falls(Blocks.STONE.stateId))
    }

    /** parity: `FallingBlockEntity`, which lands on the first block that will not give way. */
    @Test
    fun sandOverAHoleFallsUntilItLands() {
        world.state.setBlockState(2, 64, 2, Blocks.STONE.stateId)
        val sand = state("sand")
        world.state.setBlockState(2, 70, 2, sand)

        val entity = assertNotNull(FallingBlocks.beginFalling(world, 2, 70, 2) { nextEntityId++ })
        assertEquals(
            Blocks.AIR.stateId,
            world.state.getBlockState(2, 70, 2),
            "the block leaves as it becomes an entity",
        )

        var landed: BlockChange? = null
        repeat(200) {
            if (landed == null) landed = FallingBlocks.tick(world, entity)
        }

        val where = assertNotNull(landed, "it should have landed")
        assertEquals(65, where.y, "resting on the stone at 64")
        assertEquals(sand, world.state.getBlockState(2, 65, 2))
        assertTrue(entity.removed)
    }

    @Test
    fun sandWithSomethingUnderItStaysWhereItIs() {
        world.state.setBlockState(4, 64, 2, Blocks.STONE.stateId)
        world.state.setBlockState(4, 65, 2, state("sand"))

        assertNull(FallingBlocks.beginFalling(world, 4, 65, 2) { nextEntityId++ })
        assertEquals(state("sand"), world.state.getBlockState(4, 65, 2))
    }

    /**
     * Sand put down with nothing under it falls too, not only sand that loses its floor.
     *
     * It used to sit there: the neighbour update asked every block around the change whether it
     * should fall, and never the one that had just arrived.
     */
    @Test
    fun sandPlacedInMidAirIsAskedToFallAsWell() {
        world.state.setBlockState(8, 70, 2, state("sand"))
        NeighbourUpdates.afterChange(world, 8, 70, 2) { _, _, _, _ -> }

        assertTrue(world.state.dueBlockTicks().isEmpty())
        val due = world.state.dueBlockTicks()
        assertEquals(listOf(listOf(8, 70, 2)), due.map { it.toList() }, "the sand itself should have been asked")
    }

    /** Digging out from under a pile is what asks for the tick that starts the fall. */
    @Test
    fun diggingUnderSandSchedulesItsFall() {
        world.state.setBlockState(6, 64, 2, Blocks.STONE.stateId)
        world.state.setBlockState(6, 65, 2, Blocks.STONE.stateId)
        world.state.setBlockState(6, 66, 2, state("sand"))

        world.state.setBlockState(6, 65, 2, Blocks.AIR.stateId)
        NeighbourUpdates.afterChange(world, 6, 65, 2) { _, _, _, _ -> }

        // The tick is two out, so nothing is due yet and one more advance brings it.
        assertTrue(world.state.dueBlockTicks().isEmpty())
        val due = world.state.dueBlockTicks()
        assertEquals(1, due.size, "the sand above should have asked to be looked at")
        assertEquals(listOf(6, 66, 2), due.single().toList())
    }
}
