package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
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

/** parity: `LecternBlock` and `LecternBlockEntity.getRedstoneSignal`. */
class LecternsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("lectern", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/lectern",
            Logger("LecternsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun lecternAt(
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
                    Identifier.vanilla("lectern"),
                    mapOf("facing" to "north", "has_book" to "false", "powered" to "false"),
                ),
            ),
        )
    }

    private fun propertiesAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)

    private val book = Identifier.vanilla("written_book")

    @Test
    fun aBookGoesOnAndComesBackOff() {
        lecternAt(4, 70, 4)

        assertNotNull(Lecterns.placeBook(world, 4, 70, 4, book, pages = 5))
        assertEquals("true", propertiesAt(4, 70, 4)["has_book"])
        assertNull(Lecterns.placeBook(world, 4, 70, 4, book), "one book at a time")

        val (change, taken) = assertNotNull(Lecterns.takeBook(world, 4, 70, 4))
        assertEquals(book, taken)
        assertEquals("false", propertiesAt(4, 70, 4)["has_book"])
        assertEquals("false", BlockStateDefinitions.propertiesOf(change.stateId)["has_book"])
    }

    /** parity: `signalPageChange`, the two-tick pulse that makes a lectern a redstone component. */
    @Test
    fun turningAPagePulses() {
        lecternAt(6, 70, 6)
        Lecterns.placeBook(world, 6, 70, 6, book, pages = 3)

        assertNotNull(Lecterns.turnPage(world, 6, 70, 6, page = 1))

        assertEquals("true", propertiesAt(6, 70, 6)["powered"])
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(pending.any { it.localX == 6 && it.type.path == "lectern" && it.delay == 2 }, "two ticks: $pending")

        assertNotNull(Lecterns.endPulse(world, 6, 70, 6))
        assertEquals("false", propertiesAt(6, 70, 6)["powered"])
        assertNull(Lecterns.endPulse(world, 6, 70, 6), "and it stays quiet")
    }

    /** parity: `LecternBlock.getSignal`, fifteen while the pulse lasts. */
    @Test
    fun aPulsingLecternFeedsRedstone() {
        lecternAt(8, 70, 8)
        Lecterns.placeBook(world, 8, 70, 8, book, pages = 3)
        Lecterns.turnPage(world, 8, 70, 8, page = 1)

        val stateId = checkNotNull(world.state.getBlockState(8, 70, 8))
        assertEquals(15, Redstone.signalOf(stateId, BlockFace.NORTH))
        assertEquals(15, Redstone.directSignalOf(stateId, BlockFace.UP), "strongly into the block below")
    }

    /**
     * parity: `getRedstoneSignal`: `floor(page / (pages - 1) * 14) + 1`, so the first page reads one
     * and the last reads fifteen.
     */
    @Test
    fun aComparatorReadsHowFarThroughTheBookItIs() {
        lecternAt(10, 70, 10)
        Lecterns.placeBook(world, 10, 70, 10, book, pages = 15)

        assertEquals(1, Containers.analogSignal(world, 10, 70, 10), "the first page")

        Lecterns.turnPage(world, 10, 70, 10, page = 7)
        assertEquals(8, Containers.analogSignal(world, 10, 70, 10), "halfway through")

        Lecterns.turnPage(world, 10, 70, 10, page = 14)
        assertEquals(15, Containers.analogSignal(world, 10, 70, 10), "the last page")

        Lecterns.takeBook(world, 10, 70, 10)
        assertEquals(0, Containers.analogSignal(world, 10, 70, 10), "and an empty lectern reads nothing")
    }

    @Test
    fun onlyABookGoesOnIt() {
        assertTrue(Lecterns.isBook(Identifier.vanilla("written_book")))
        assertTrue(Lecterns.isBook(Identifier.vanilla("writable_book")))
        assertTrue(!Lecterns.isBook(Identifier.vanilla("book")), "a plain book is not one you can read")
        assertTrue(!Lecterns.isBook(Identifier.vanilla("stone")))
    }
}
