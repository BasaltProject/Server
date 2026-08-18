package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.protocol.play.SoundSource

/**
 * The chiseled bookshelf: six slots, one book each, chosen by where the click landed.
 *
 * parity: `ChiseledBookShelfBlock` over `ChiseledBookShelfBlockEntity`. What a comparator reads is
 * not how full it is but *which* slot was touched last, plus one, which is what makes it the only
 * container that can address six different signals.
 */
public object ChiseledBookshelves {
    public val BLOCK: Identifier = Identifier.vanilla("chiseled_bookshelf")

    /** parity: `MAX_BOOKS_IN_STORAGE`, laid out two rows of three. */
    public const val SIZE: Int = 6
    public const val ROWS: Int = 2
    public const val COLUMNS: Int = 3

    private const val LAST_SLOT_KEY = "last_interacted_slot"

    public fun isBookshelf(path: String?): Boolean = path == BLOCK.path

    /** Which slot a click at these coordinates lands on, or null when it was not on the front. */
    public fun slotAt(
        stateId: Int,
        face: String,
        clickX: Float,
        clickY: Float,
        clickZ: Float,
    ): Int? {
        val facing = BlockStateDefinitions.propertiesOf(stateId)["facing"] ?: return null
        return SelectableSlots.hitSlot(facing, face, clickX, clickY, clickZ, ROWS, COLUMNS)
    }

    /** What a book being put in or taken out leaves behind: the state, and what it sounded like. */
    public class Interaction(
        public val change: BlockChange,
        public val taken: Identifier?,
    )

    /**
     * parity: `addBook`, which only takes a book into a slot that is empty, and marks that slot as
     * the one a comparator reads.
     */
    public fun putBook(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        slot: Int,
        book: Identifier,
    ): Interaction? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (BlockStateDefinitions.blockOf(stateId) != BLOCK) return null
        if (slot !in 0 until SIZE) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["slot_${slot}_occupied"] == "true") return null
        val entity = entityAt(world, x, y, z) ?: return null
        val slots = BlockInventory.read(entity.data, SIZE)
        slots[slot] = ItemStack(book, 1)
        BlockInventory.write(entity.data, slots)
        entity.data.putInt(LAST_SLOT_KEY, slot)
        world.state.chunkIfLoaded(x shr 4, z shr 4)?.markDirty()
        val turned = occupy(world, x, y, z, stateId, slot, true) ?: return null
        val enchanted = book.path == "enchanted_book"
        return Interaction(
            BlockChange(
                x,
                y,
                z,
                turned,
                listOf(
                    BlockEffect.Sound(
                        if (enchanted) {
                            "block.chiseled_bookshelf.insert.enchanted"
                        } else {
                            "block.chiseled_bookshelf.insert"
                        },
                        SoundSource.BLOCKS,
                        1f,
                        1f,
                    ),
                ),
            ),
            taken = null,
        )
    }

    /** parity: `removeBook`, the other half, which hands the book back. */
    public fun takeBook(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        slot: Int,
    ): Interaction? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (BlockStateDefinitions.blockOf(stateId) != BLOCK) return null
        if (slot !in 0 until SIZE) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["slot_${slot}_occupied"] != "true") return null
        val entity = entityAt(world, x, y, z) ?: return null
        val slots = BlockInventory.read(entity.data, SIZE)
        val book = slots[slot]?.item ?: Identifier.vanilla("book")
        slots[slot] = null
        BlockInventory.write(entity.data, slots)
        entity.data.putInt(LAST_SLOT_KEY, slot)
        world.state.chunkIfLoaded(x shr 4, z shr 4)?.markDirty()
        val turned = occupy(world, x, y, z, stateId, slot, false) ?: return null
        val enchanted = book.path == "enchanted_book"
        return Interaction(
            BlockChange(
                x,
                y,
                z,
                turned,
                listOf(
                    BlockEffect.Sound(
                        if (enchanted) {
                            "block.chiseled_bookshelf.pickup.enchanted"
                        } else {
                            "block.chiseled_bookshelf.pickup"
                        },
                        SoundSource.BLOCKS,
                        1f,
                        1f,
                    ),
                ),
            ),
            taken = book,
        )
    }

    /** parity: `getAnalogOutputSignal`, the last slot touched plus one, so an untouched shelf reads nothing. */
    public fun comparatorSignal(data: NbtCompound): Int = ((data[LAST_SLOT_KEY] as? NbtInt)?.value ?: -1) + 1

    private fun occupy(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        slot: Int,
        occupied: Boolean,
    ): Int? {
        val turned =
            BlockStateDefinitions.stateIdOf(BLOCK, mapOf("slot_${slot}_occupied" to occupied.toString()), stateId)
                ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return turned
    }

    private fun entityAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): ChunkBlockEntity? {
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        return chunk.blockEntityAt(x and 15, y, z and 15)
            ?: ChunkBlockEntity(x and 15, y, z and 15, BLOCK, NbtCompound()).also { chunk.setBlockEntity(it) }
    }
}
