package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `SelectableSlotContainer`, the chiseled bookshelf and the shelf that share it. */
class SelectableSlotContainersTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("slots", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/slots",
            Logger("SelectableSlotContainersTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private val book = Identifier.vanilla("book")

    private fun state(
        name: String,
        properties: Map<String, String>,
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun bookshelf(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val occupied = (0 until ChiseledBookshelves.SIZE).associate { "slot_${it}_occupied" to "false" }
        val stateId = state("chiseled_bookshelf", occupied + mapOf("facing" to "south"))
        world.state.setBlockState(x, y, z, stateId)
        return stateId
    }

    /** parity: `getHitSlot`, a click low on the left is slot three and high on the right is slot two. */
    @Test
    fun theClickDecidesTheSlot() {
        val stateId = bookshelf(2, 70, 2)

        // A south-facing shelf clicked on its south face: x runs left to right as drawn.
        assertEquals(0, ChiseledBookshelves.slotAt(stateId, "south", 0.1f, 0.9f, 1f))
        assertEquals(2, ChiseledBookshelves.slotAt(stateId, "south", 0.9f, 0.9f, 1f))
        assertEquals(3, ChiseledBookshelves.slotAt(stateId, "south", 0.1f, 0.1f, 1f))
        assertEquals(5, ChiseledBookshelves.slotAt(stateId, "south", 0.9f, 0.1f, 1f))
    }

    /** parity: a shelf poked on any face but the one it looks out of takes nothing. */
    @Test
    fun onlyTheFrontAnswers() {
        val stateId = bookshelf(5, 70, 2)

        assertNull(ChiseledBookshelves.slotAt(stateId, "north", 0.5f, 0.5f, 0f))
        assertNull(ChiseledBookshelves.slotAt(stateId, "up", 0.5f, 1f, 0.5f))
    }

    /** parity: `addBook`/`removeBook`, one book per slot, and the state says which are full. */
    @Test
    fun aBookGoesInAndComesBackOut() {
        bookshelf(8, 70, 2)

        val put = assertNotNull(ChiseledBookshelves.putBook(world, 8, 70, 2, slot = 4, book = book))
        assertEquals("true", BlockStateDefinitions.propertiesOf(put.change.stateId)["slot_4_occupied"])
        assertNull(ChiseledBookshelves.putBook(world, 8, 70, 2, slot = 4, book = book), "the slot is taken")

        val taken = assertNotNull(ChiseledBookshelves.takeBook(world, 8, 70, 2, slot = 4))
        assertEquals(book, taken.taken)
        assertEquals("false", BlockStateDefinitions.propertiesOf(taken.change.stateId)["slot_4_occupied"])
    }

    /** parity: `getAnalogOutputSignal`, which reads the last slot touched and not how full it is. */
    @Test
    fun aComparatorReadsTheLastSlotTouched() {
        bookshelf(11, 70, 2)
        assertEquals(0, Containers.analogSignal(world, 11, 70, 2), "nothing has been touched yet")

        ChiseledBookshelves.putBook(world, 11, 70, 2, slot = 0, book = book)
        assertEquals(1, Containers.analogSignal(world, 11, 70, 2))

        ChiseledBookshelves.putBook(world, 11, 70, 2, slot = 5, book = book)
        assertEquals(6, Containers.analogSignal(world, 11, 70, 2))

        // Taking one back out still counts as touching that slot.
        ChiseledBookshelves.takeBook(world, 11, 70, 2, slot = 0)
        assertEquals(1, Containers.analogSignal(world, 11, 70, 2))
    }

    private fun shelf(
        x: Int,
        y: Int,
        z: Int,
        powered: Boolean = false,
    ): Int {
        val stateId =
            state(
                "oak_shelf",
                mapOf(
                    "facing" to "south",
                    "powered" to powered.toString(),
                    "side_chain" to "unconnected",
                    "waterlogged" to "false",
                ),
            )
        world.state.setBlockState(x, y, z, stateId)
        return stateId
    }

    /** parity: `swapSingleItem`, which is a swap and not a put: what was there comes back. */
    @Test
    fun aShelfSwapsWhatIsHeldForWhatIsOnIt() {
        shelf(2, 73, 2)
        val emerald = ItemStack(Identifier.vanilla("emerald"), 1)
        val diamond = ItemStack(Identifier.vanilla("diamond"), 1)

        assertNull(assertNotNull(Shelves.swap(world, 2, 73, 2, 1, emerald)).removed, "nothing was there")
        val swapped = assertNotNull(Shelves.swap(world, 2, 73, 2, 1, diamond))
        assertEquals(emerald.item, assertNotNull(swapped.removed).item)

        val emptied = assertNotNull(Shelves.swap(world, 2, 73, 2, 1, null))
        assertEquals(diamond.item, assertNotNull(emptied.removed).item)
        assertEquals(
            "block.shelf.take_item",
            emptied.effects
                .filterIsInstance<BlockEffect.Sound>()
                .single()
                .name,
        )
    }

    /** parity: `ShelfBlock.getAnalogOutputSignal`, one bit per slot, read only from behind. */
    @Test
    fun aComparatorReadsTheShelfAsThreeBits() {
        shelf(5, 73, 2)
        Shelves.swap(world, 5, 73, 2, 0, ItemStack(Identifier.vanilla("emerald"), 1))
        Shelves.swap(world, 5, 73, 2, 2, ItemStack(Identifier.vanilla("emerald"), 1))

        // A south-facing shelf is read from the north, which is its back.
        assertEquals(0b101, Containers.analogSignal(world, 5, 73, 2, from = "north"))
        assertEquals(0, Containers.analogSignal(world, 5, 73, 2, from = "east"), "the side reads nothing")
    }

    /** parity: `updateSelfAndNeighborsOnPoweringUp`, which joins at most three shelves into one chain. */
    @Test
    fun poweringShelvesJoinsThemIntoAChain() {
        for (x in 8..10) {
            shelf(x, 73, 5)
            world.state.setBlockState(x, 72, 5, state("redstone_block", emptyMap()))
        }
        // Each one joins as the signal reaches it, which is what a neighbour update does.
        val changes = mutableListOf<BlockChange>()
        for (x in 8..10) Shelves.reactToSignal(world, x, 73, 5, changes)

        assertEquals(3, Shelves.chainAt(world, 9, 73, 5).size)
        assertEquals(
            "center",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(9, 73, 5)))["side_chain"],
        )

        // parity: a fourth shelf does not fit, because the chain stops at three.
        shelf(11, 73, 5)
        world.state.setBlockState(11, 72, 5, state("redstone_block", emptyMap()))
        Shelves.reactToSignal(world, 11, 73, 5, changes)
        assertEquals(1, Shelves.chainAt(world, 11, 73, 5).size)
    }

    /** parity: `swapHotbar`, which lines the chain up with the end of the hotbar. */
    @Test
    fun aPoweredShelfSwapsTheWholeHotbar() {
        shelf(2, 76, 2, powered = true)
        val hotbar =
            Array<ItemStack?>(9) { index ->
                if (index >= 6) ItemStack(Identifier.vanilla("emerald"), index + 1) else null
            }

        assertTrue(Shelves.swapHotbar(world, 2, 76, 2, hotbar))

        assertTrue(hotbar.drop(6).all { it == null }, "the last three went onto the shelf")
        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        val entity = checkNotNull(chunk.blockEntityAt(2, 76, 2))
        val slots = BlockInventory.read(entity.data, Shelves.SIZE)
        assertEquals(7, checkNotNull(slots[0]).count)
        assertEquals(9, checkNotNull(slots[2]).count)
    }
}
