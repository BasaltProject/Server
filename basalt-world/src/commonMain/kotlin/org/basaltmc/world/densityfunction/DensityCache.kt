package org.basaltmc.world.densityfunction

/**
 * // parity: net.minecraft.world.level.levelgen.NoiseChunk's own `FlatCache` and `Cache2D`, the
 * caches its visitor wraps [Marker] subtrees in. Vanilla builds a fresh `NoiseChunk` per chunk and
 * rewrites the router tree into it; this project's router is an immutable tree shared by every
 * generation worker, so the mutable state lives here instead and travels down the tree inside
 * [FunctionContext.cache]. One instance belongs to exactly one chunk on exactly one thread, so
 * nothing here needs a lock.
 *
 * [flatCache] is not a memoization of what the wrapped function would otherwise return: vanilla
 * quantizes the query to the quart grid and samples at `y = 0`, and every position inside that
 * quart gets that one value. Reproducing that quantization is the point, not a shortcut around it.
 */
public class DensityCache(
    chunkX: Int,
    chunkZ: Int,
    slotCount: Int,
) {
    private val firstQuartX = chunkX * QUARTS_PER_CHUNK
    private val firstQuartZ = chunkZ * QUARTS_PER_CHUNK

    private val flatValues = arrayOfNulls<DoubleArray>(slotCount)
    private val flatPresent = arrayOfNulls<BooleanArray>(slotCount)
    private val lastSeen2d = BooleanArray(slotCount)
    private val lastPos2d = LongArray(slotCount)
    private val lastValue2d = DoubleArray(slotCount)

    /**
     * // parity: NoiseChunk.FlatCache.compute. Out-of-range positions fall through to [function]
     * evaluated at the real context, exactly as vanilla's own bounds check does; vanilla fills its
     * grid up front, this fills each cell on first use, which computes strictly fewer values and
     * the same ones.
     */
    public fun flatCache(
        slot: Int,
        context: FunctionContext,
        function: DensityFunction,
    ): Double {
        val gridX = (context.blockX shr 2) - firstQuartX
        val gridZ = (context.blockZ shr 2) - firstQuartZ
        if (slot >= flatValues.size || gridX < 0 || gridZ < 0 || gridX >= GRID_SIZE || gridZ >= GRID_SIZE) {
            return function.compute(context)
        }
        val values = flatValues[slot] ?: DoubleArray(GRID_SIZE * GRID_SIZE).also { flatValues[slot] = it }
        val present = flatPresent[slot] ?: BooleanArray(GRID_SIZE * GRID_SIZE).also { flatPresent[slot] = it }
        val index = gridX + gridZ * GRID_SIZE
        if (!present[index]) {
            val blockX = (firstQuartX + gridX) shl 2
            val blockZ = (firstQuartZ + gridZ) shl 2
            values[index] = function.compute(FunctionContext(blockX, 0, blockZ, this))
            present[index] = true
        }
        return values[index]
    }

    /** // parity: NoiseChunk.Cache2D.compute. Exact last-position memoization, no quantization. */
    public fun cache2d(
        slot: Int,
        context: FunctionContext,
        function: DensityFunction,
    ): Double {
        if (slot >= lastPos2d.size) return function.compute(context)
        val packed = (context.blockX.toLong() and 0xFFFFFFFFL) or (context.blockZ.toLong() shl 32)
        if (lastSeen2d[slot] && lastPos2d[slot] == packed) return lastValue2d[slot]
        val value = function.compute(context)
        lastSeen2d[slot] = true
        lastPos2d[slot] = packed
        lastValue2d[slot] = value
        return value
    }

    private companion object {
        const val QUARTS_PER_CHUNK = 4

        // parity: NoiseChunk's own `noiseSizeXZ + 1`, with the real Overworld cellWidth of 4 (which
        // makes the noise grid and the quart grid the same thing).
        const val GRID_SIZE = QUARTS_PER_CHUNK + 1
    }
}
