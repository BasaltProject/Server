package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtString
import kotlin.math.floor

/**
 * The lectern: a book on a stand, and the page somebody is on.
 *
 * parity: `LecternBlock` over `LecternBlockEntity`. Turning a page is a two-tick pulse of redstone
 * (`signalPageChange`), and a comparator behind it reads how far through the book the reader is:
 * `floor(page / (pages - 1) * 14) + 1`, so the first page is one and the last is fifteen.
 */
public object Lecterns {
    public val BLOCK: Identifier = Identifier.vanilla("lectern")

    /** parity: `PAGE_CHANGE_IMPULSE_TICKS`, the two ticks the pulse lasts. */
    public const val PULSE_TICKS: Int = 2

    private const val BOOK_KEY = "Book"
    private const val PAGE_KEY = "Page"
    private const val PAGES_KEY = "Pages"

    /** parity: a written book carries its own pages; a plain one is a single page. */
    public const val DEFAULT_PAGES: Int = 1

    /** parity: `tryPlaceBook`, which only takes one when the stand is empty. */
    public fun placeBook(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        book: Identifier,
        pages: Int = DEFAULT_PAGES,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block != BLOCK) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["has_book"] == "true") return null
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        val entity =
            chunk.blockEntityAt(x and 15, y, z and 15)
                ?: ChunkBlockEntity(x and 15, y, z and 15, BLOCK, NbtCompound()).also { chunk.setBlockEntity(it) }
        entity.data.putString(BOOK_KEY, book.toString())
        entity.data.putInt(PAGE_KEY, 0)
        entity.data.putInt(PAGES_KEY, maxOf(1, pages))
        chunk.markDirty()
        // parity: `resetBookState`, which clears the pulse as it puts the book down.
        val turned =
            BlockStateDefinitions.stateIdOf(
                block,
                mapOf("has_book" to "true", "powered" to "false"),
                stateId,
            ) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /** parity: `resetBookState(false)` plus the book itself, which is what taking it back gives. */
    public fun takeBook(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Pair<BlockChange, Identifier>? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block != BLOCK) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["has_book"] != "true") return null
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return null
        val book = (entity.data[BOOK_KEY] as? NbtString)?.value ?: return null
        entity.data.remove(BOOK_KEY)
        entity.data.remove(PAGE_KEY)
        entity.data.remove(PAGES_KEY)
        chunk.markDirty()
        val turned =
            BlockStateDefinitions.stateIdOf(
                block,
                mapOf("has_book" to "false", "powered" to "false"),
                stateId,
            ) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned) to Identifier.parse(book)
    }

    /**
     * parity: `signalPageChange`: the page moves, the block pulses for two ticks, and the comparator
     * behind it reads the new page. Answers the change, or null when there is no book to turn.
     */
    public fun turnPage(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        page: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block != BLOCK) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["has_book"] != "true") return null
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return null
        val pages = (entity.data[PAGES_KEY] as? NbtInt)?.value ?: DEFAULT_PAGES
        entity.data.putInt(PAGE_KEY, page.coerceIn(0, maxOf(0, pages - 1)))
        chunk.markDirty()
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("powered" to "true"), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        world.state.scheduleBlockTick(x, y, z, BLOCK, PULSE_TICKS)
        return BlockChange(x, y, z, turned)
    }

    /** The other half of the pulse: two ticks later the lectern goes quiet again. */
    public fun endPulse(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block != BLOCK) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["powered"] != "true") return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("powered" to "false"), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /**
     * parity: `LecternBlockEntity.getRedstoneSignal`: how far through the book the reader is, over
     * fourteen, plus one for having a book at all.
     */
    public fun comparatorSignal(data: NbtCompound): Int {
        val hasBook = (data[BOOK_KEY] as? NbtString) != null
        val pages = (data[PAGES_KEY] as? NbtInt)?.value ?: DEFAULT_PAGES
        val page = (data[PAGE_KEY] as? NbtInt)?.value ?: 0
        val progress = if (pages > 1) page.toFloat() / (pages - 1) else 1f
        return floor(progress * 14f).toInt() + if (hasBook) 1 else 0
    }

    /** parity: `LecternBlock.tryPlaceBook`'s own check, which takes a book and nothing else. */
    public fun isBook(item: Identifier): Boolean = item.path == "written_book" || item.path == "writable_book"
}
