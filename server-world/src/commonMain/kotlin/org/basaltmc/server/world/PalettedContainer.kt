package org.basaltmc.server.world

import kotlin.concurrent.Volatile

/**
 * // parity: net.minecraft.world.level.chunk.PalettedContainer, as storage only.
 *
 * Holds [size] values while paying for the distinct ones: all-air keeps a one-entry palette and no
 * data array, and only hundreds of distinct states cost what a plain `IntArray` always did. That
 * is what makes chunks affordable to keep in memory, where a flat `IntArray(4096)` per section put
 * one chunk at roughly 390 KB.
 *
 * One writer, any number of readers. Connection threads read a chunk to send it while the world is
 * still being written to, so the width, the palette and the data array are swapped as one
 * [Storage]: read separately, a widening write handed a reader the new width with the old array and
 * it indexed off the end.
 */
public class PalettedContainer(
    private val size: Int,
    initialValue: Int = 0,
) {
    /** 0 bits means every entry is [palette]`[0]`, in which case [words] stays null and costs nothing. */
    private class Storage(
        val bits: Int,
        val palette: IntArray,
        val paletteSize: Int,
        val words: LongArray?,
    )

    @Volatile
    private var storage =
        Storage(0, IntArray(INITIAL_PALETTE_CAPACITY).also { it[0] = initialValue }, 1, null)

    public operator fun get(index: Int): Int {
        val current = storage
        val words = current.words ?: return current.palette[0]
        val raw = readRaw(words, current.bits, index)
        // A reader can catch an entry written after its own snapshot was taken.
        return if (raw < current.paletteSize) current.palette[raw] else current.palette[0]
    }

    /** Returns whether the stored value actually changed, so callers can track modifications without re-reading. */
    public operator fun set(
        index: Int,
        value: Int,
    ): Boolean {
        val current = storage
        if (current.words == null && current.palette[0] == value) return false
        if (get(index) == value) return false
        write(index, indexOfOrAdd(value))
        return true
    }

    /** Every value, expanded. Allocates: for the save and network paths, which need a flat view once. */
    public fun toIntArray(): IntArray = IntArray(size).also { copyInto(it) }

    /**
     * Expands into [dest], which must hold at least [size] values.
     *
     * Takes the storage snapshot once rather than once per value: [get] reads a volatile field, and
     * a save expanding every section of every chunk did that four thousand times a section.
     */
    public fun copyInto(dest: IntArray) {
        require(dest.size >= size) { "expected room for $size values, got ${dest.size}" }
        val current = storage
        val words = current.words
        if (words == null) {
            dest.fill(current.palette[0], 0, size)
            return
        }
        val bits = current.bits
        val palette = current.palette
        val paletteSize = current.paletteSize
        for (i in 0 until size) {
            val raw = readRaw(words, bits, i)
            dest[i] = if (raw < paletteSize) palette[raw] else palette[0]
        }
    }

    /** Replaces the whole contents, re-deriving the palette from [values]. */
    public fun setAll(values: IntArray) {
        require(values.size == size) { "expected $size values, got ${values.size}" }
        storage = Storage(0, IntArray(INITIAL_PALETTE_CAPACITY).also { it[0] = values[0] }, 1, null)
        for (i in values.indices) set(i, values[i])
    }

    /** How many distinct values are stored, which is what decides the entry width. */
    public val distinctValueCount: Int get() = storage.paletteSize

    /**
     * Whether any stored value satisfies [predicate], answered from the palette alone.
     *
     * This is what makes a whole-world search affordable: a section keeps one entry per *distinct*
     * value, so asking it is a handful of comparisons rather than four thousand, and a section that
     * cannot hold what is being looked for is skipped without reading a single packed word.
     */
    public fun anyValue(predicate: (Int) -> Boolean): Boolean {
        val current = storage
        for (i in 0 until current.paletteSize) if (predicate(current.palette[i])) return true
        return false
    }

    /** Approximate bytes this container keeps resident: palette plus packed words, headers ignored. */
    public val residentBytes: Int
        get() {
            val current = storage
            return current.palette.size * Int.SIZE_BYTES + (current.words?.size ?: 0) * Long.SIZE_BYTES
        }

    /** Bits each entry occupies; 0 while every entry is identical and no data array exists. */
    public val bitsPerValue: Int get() = storage.bits

    /**
     * The palette slot for [value], adding it and widening the entries if it is new.
     *
     * Publishes the new [Storage] before the caller writes the slot into the data, so a reader
     * never meets an index its palette does not have yet.
     */
    private fun indexOfOrAdd(value: Int): Int {
        val current = storage
        for (i in 0 until current.paletteSize) if (current.palette[i] == value) return i

        val palette =
            if (current.paletteSize == current.palette.size) {
                current.palette.copyOf(current.palette.size * 2)
            } else {
                current.palette
            }
        palette[current.paletteSize] = value
        val paletteSize = current.paletteSize + 1

        val needed = bitsFor(paletteSize)
        if (needed <= current.bits) {
            storage = Storage(current.bits, palette, paletteSize, current.words)
            return paletteSize - 1
        }

        val widened = needed.coerceAtLeast(MIN_BITS_PER_ENTRY)
        val words = LongArray((size * widened + 63) / 64)
        val previous = current.words
        // Palette slots are unchanged, so re-packing the indices at the wider width moves no values.
        if (previous != null) {
            for (i in 0 until size) writeInto(words, widened, i, readRaw(previous, current.bits, i))
        }
        storage = Storage(widened, palette, paletteSize, words)
        return paletteSize - 1
    }

    private fun write(
        index: Int,
        paletteIndex: Int,
    ) {
        val current = storage
        val words = current.words ?: return
        writeInto(words, current.bits, index, paletteIndex)
    }

    private companion object {
        const val INITIAL_PALETTE_CAPACITY = 4

        // Matching vanilla's own floor for an indirect palette: narrower entries save so little that
        // they are not worth the extra widths to handle.
        const val MIN_BITS_PER_ENTRY = 4

        fun bitsFor(entries: Int): Int {
            var bits = 0
            while ((1 shl bits) < entries) bits++
            return bits
        }

        fun readRaw(
            words: LongArray,
            bits: Int,
            index: Int,
        ): Int {
            val bitIndex = index * bits
            val wordIndex = bitIndex ushr 6
            val bitOffset = bitIndex and 63
            val mask = (1L shl bits) - 1L
            val available = 64 - bitOffset
            val raw =
                if (available >= bits) {
                    (words[wordIndex] ushr bitOffset) and mask
                } else {
                    // An entry straddling two words: the low bits finish in the next one.
                    ((words[wordIndex] ushr bitOffset) or (words[wordIndex + 1] shl available)) and mask
                }
            return raw.toInt()
        }

        fun writeInto(
            words: LongArray,
            bits: Int,
            index: Int,
            paletteIndex: Int,
        ) {
            val bitIndex = index * bits
            val wordIndex = bitIndex ushr 6
            val bitOffset = bitIndex and 63
            val mask = (1L shl bits) - 1L
            val value = paletteIndex.toLong() and mask
            val available = 64 - bitOffset
            words[wordIndex] = (words[wordIndex] and (mask shl bitOffset).inv()) or (value shl bitOffset)
            if (available < bits) {
                words[wordIndex + 1] = (words[wordIndex + 1] and (mask ushr available).inv()) or (value ushr available)
            }
        }
    }
}
