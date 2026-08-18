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

/** parity: `DecoratedPotBlock` over `DecoratedPotBlockEntity`, a container of exactly one slot. */
class DecoratedPotsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("pots", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/pots",
            Logger("DecoratedPotsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private val emerald = Identifier.vanilla("emerald")
    private val diamond = Identifier.vanilla("diamond")

    private fun pot(
        x: Int,
        y: Int,
        z: Int,
    ) {
        world.state.setBlockState(
            x,
            y,
            z,
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    DecoratedPots.BLOCK,
                    mapOf("facing" to "north", "waterlogged" to "false"),
                ),
            ),
        )
    }

    /** parity: `useItemOn`, which takes one item at a time and stacks it with what is already there. */
    @Test
    fun aPotTakesOneItemAtATime() {
        pot(2, 70, 2)

        assertNotNull(DecoratedPots.insert(world, 2, 70, 2, emerald))
        assertNotNull(DecoratedPots.insert(world, 2, 70, 2, emerald))

        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        val entity = checkNotNull(chunk.blockEntityAt(2, 70, 2))
        assertEquals(2, checkNotNull(DecoratedPots.contents(entity)).count)
    }

    /** parity: the pot refuses a second kind of item, because it has one slot and no screen. */
    @Test
    fun aPotHoldsOneKindOfThing() {
        pot(5, 70, 2)
        assertNotNull(DecoratedPots.insert(world, 5, 70, 2, emerald))

        assertNull(DecoratedPots.insert(world, 5, 70, 2, diamond), "and the pot only rocks")
    }

    /** parity: the insert sound rises in pitch with how full the pot is. */
    @Test
    fun theSoundRisesAsItFills() {
        pot(8, 70, 2)
        val first = checkNotNull(DecoratedPots.insert(world, 8, 70, 2, emerald))
        repeat(20) { DecoratedPots.insert(world, 8, 70, 2, emerald) }
        val later = checkNotNull(DecoratedPots.insert(world, 8, 70, 2, emerald))

        val firstPitch = first.filterIsInstance<BlockEffect.Sound>().single().pitch
        val laterPitch = later.filterIsInstance<BlockEffect.Sound>().single().pitch
        assertTrue(laterPitch > firstPitch, "$laterPitch should be over $firstPitch")
    }

    /** parity: `getRedstoneSignalFromBlockEntity` over one slot, so a full pot reads fifteen. */
    @Test
    fun aComparatorReadsHowFullItIs() {
        pot(11, 70, 2)
        assertEquals(0, Containers.analogSignal(world, 11, 70, 2))

        repeat(64) { DecoratedPots.insert(world, 11, 70, 2, emerald) }

        assertEquals(15, Containers.analogSignal(world, 11, 70, 2))
    }

    /** A hopper under it takes one item at a time, which is what makes a pot a filter. */
    @Test
    fun oneItemComesBackOutAtATime() {
        pot(14, 70, 2)
        DecoratedPots.insert(world, 14, 70, 2, emerald)
        DecoratedPots.insert(world, 14, 70, 2, emerald)

        val taken = checkNotNull(DecoratedPots.takeOne(world, 14, 70, 2))
        assertEquals(emerald, taken.item)
        assertEquals(1, taken.count)

        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        val entity = checkNotNull(chunk.blockEntityAt(14, 70, 2))
        assertEquals(1, checkNotNull(DecoratedPots.contents(entity)).count)
    }
}
