package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** parity: `ShulkerBoxBlock.getDrops` into `BlockItem.updateCustomBlockEntityTag` and back. */
class ShulkersTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("shulkers", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/shulkers",
            Logger("ShulkersTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun box(
        x: Int,
        y: Int,
        z: Int,
    ): ChunkBlockEntity {
        val block = Identifier.vanilla("shulker_box")
        world.state.setBlockState(
            x,
            y,
            z,
            checkNotNull(BlockStateDefinitions.stateIdOf(block, mapOf("facing" to "up"))),
        )
        val chunk = checkNotNull(world.state.chunkIfLoaded(x shr 4, z shr 4))
        return ChunkBlockEntity(x and 15, y, z and 15, block, NbtCompound()).also { chunk.setBlockEntity(it) }
    }

    /** parity: the contents come out with the item and go back in with the block. */
    @Test
    fun aBoxKeepsWhatIsInItAcrossABreak() {
        val entity = box(2, 70, 2)
        val slots = arrayOfNulls<ItemStack>(Shulkers.SIZE)
        slots[0] = ItemStack(Identifier.vanilla("emerald"), 12)
        slots[26] = ItemStack(Identifier.vanilla("diamond"), 3)
        BlockInventory.write(entity.data, slots)

        val carried = assertNotNull(Shulkers.contentsOf(world, 2, 70, 2))

        // Breaking it: the block, and its entity with it, are gone.
        world.state.setBlockState(2, 70, 2, org.basaltmc.server.registry.Blocks.AIR.stateId)
        assertNull(world.state.chunkIfLoaded(0, 0)?.blockEntityAt(2, 70, 2))

        box(2, 70, 2)
        Shulkers.restore(world, 2, 70, 2, carried)

        val back = checkNotNull(world.state.chunkIfLoaded(0, 0)?.blockEntityAt(2, 70, 2))
        val restored = BlockInventory.read(back.data, Shulkers.SIZE)
        assertEquals(12, checkNotNull(restored[0]).count)
        assertEquals(Identifier.vanilla("diamond"), checkNotNull(restored[26]).item)
    }

    /** An empty box carries nothing, so an empty one does not become a stack with baggage. */
    @Test
    fun anEmptyBoxCarriesNothing() {
        box(5, 70, 2)

        assertNull(Shulkers.contentsOf(world, 5, 70, 2))
    }

    /** parity: `getRedstoneSignalFromBlockEntity`, the same reading a chest gives. */
    @Test
    fun aComparatorReadsIt() {
        val entity = box(8, 70, 2)
        val slots = arrayOfNulls<ItemStack>(Shulkers.SIZE)
        for (slot in slots.indices) slots[slot] = ItemStack(Identifier.vanilla("emerald"), 64)
        BlockInventory.write(entity.data, slots)

        assertEquals(15, Containers.analogSignal(world, 8, 70, 2))
    }

    /** Every dye has one, and the plain one makes seventeen. */
    @Test
    fun everyColourIsAContainer() {
        assertEquals(17, Shulkers.BLOCKS.size)
        for (block in Shulkers.BLOCKS) {
            assertEquals(Shulkers.SIZE, Containers.SIZES[block], block)
            assertEquals(true, block in Containers.PLAIN, block)
        }
    }
}
