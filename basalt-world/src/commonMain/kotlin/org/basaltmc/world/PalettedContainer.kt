package org.basaltmc.world

/**
 * // parity: net.minecraft.world.level.chunk.PalettedContainer, as storage only.
 *
 * Holds [size] values while paying for the distinct ones: all-air keeps a one-entry palette and no
 * data array, and only hundreds of distinct states cost what a plain `IntArray` always did. That
 * is what makes chunks affordable to keep in memory, where a flat `IntArray(4096)` per section put
 * one chunk at roughly 390 KB.
 *
 * Not thread safe: a section belongs to one chunk, and chunk access is serialized a level up.
 */
public class PalettedContainer(
    private val size: Int,
    initialValue: Int = 0,
) {
    private var palette = IntArray(INITIAL_PALETTE_CAPACITY).also { it[0] = initialValue }
    private var paletteSize = 1

    /** 0 means every entry is [palette]`[0]`, in which case [data] stays null and costs nothing. */
    private var bitsPerEntry = 0
    private var data: LongArray? = null

    public operator fun get(index: Int): Int {
        val storage = data ?: return palette[0]
        val bitIndex = index * bitsPerEntry
        val wordIndex = bitIndex ushr 6
        val bitOffset = bitIndex and 63
        val mask = (1L shl bitsPerEntry) - 1L
        val available = 64 - bitOffset
        val raw =
            if (available >= bitsPerEntry) {
                (storage[wordIndex] ushr bitOffset) and mask
            } else {
                // An entry straddling two words: the low bits finish in the next one.
                ((storage[wordIndex] ushr bitOffset) or (storage[wordIndex + 1] shl available)) and mask
            }
        return palette[raw.toInt()]
    }

    /** Returns whether the stored value actually changed, so callers can track modifications without re-reading. */
    public operator fun set(
        index: Int,
        value: Int,
    ): Boolean {
        if (data == null && palette[0] == value) return false
        if (get(index) == value) return false
        write(index, indexOfOrAdd(value))
        return true
    }

    /** Every value, expanded. Allocates: for the save and network paths, which need a flat view once. */
    public fun toIntArray(): IntArray = IntArray(size) { get(it) }

    /** Replaces the whole contents, re-deriving the palette from [values]. */
    public fun setAll(values: IntArray) {
        require(values.size == size) { "expected $size values, got ${values.size}" }
        palette = IntArray(INITIAL_PALETTE_CAPACITY)
        palette[0] = values[0]
        paletteSize = 1
        bitsPerEntry = 0
        data = null
        for (i in values.indices) set(i, values[i])
    }

    /** How many distinct values are stored, which is what decides the entry width. */
    public val distinctValueCount: Int get() = paletteSize

    /** Bits each entry occupies; 0 while every entry is identical and no data array exists. */
    public val bitsPerValue: Int get() = bitsPerEntry

    private fun indexOfOrAdd(value: Int): Int {
        for (i in 0 until paletteSize) if (palette[i] == value) return i
        if (paletteSize == palette.size) palette = palette.copyOf(palette.size * 2)
        palette[paletteSize] = value
        paletteSize++
        growIfNeeded()
        return paletteSize - 1
    }

    private fun growIfNeeded() {
        val needed = bitsFor(paletteSize)
        if (needed <= bitsPerEntry) return
        val widened = needed.coerceAtLeast(MIN_BITS_PER_ENTRY)
        val previous = data
        val previousBits = bitsPerEntry
        bitsPerEntry = widened
        data = LongArray((size * widened + 63) / 64)
        if (previous == null) {
            // Was uniform: every entry already refers to palette slot 0, which is all-zero bits.
            return
        }
        // Re-pack the existing indices at the wider width. Palette slots are unchanged, so the
        // values themselves are untouched by the move.
        for (i in 0 until size) {
            write(i, readRaw(previous, previousBits, i))
        }
    }

    private fun readRaw(
        storage: LongArray,
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
                (storage[wordIndex] ushr bitOffset) and mask
            } else {
                ((storage[wordIndex] ushr bitOffset) or (storage[wordIndex + 1] shl available)) and mask
            }
        return raw.toInt()
    }

    private fun write(
        index: Int,
        paletteIndex: Int,
    ) {
        val storage = data ?: return
        val bitIndex = index * bitsPerEntry
        val wordIndex = bitIndex ushr 6
        val bitOffset = bitIndex and 63
        val mask = (1L shl bitsPerEntry) - 1L
        val value = paletteIndex.toLong() and mask
        val available = 64 - bitOffset
        storage[wordIndex] = (storage[wordIndex] and (mask shl bitOffset).inv()) or (value shl bitOffset)
        if (available < bitsPerEntry) {
            storage[wordIndex + 1] =
                (storage[wordIndex + 1] and (mask ushr available).inv()) or (value ushr available)
        }
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
    }
}
