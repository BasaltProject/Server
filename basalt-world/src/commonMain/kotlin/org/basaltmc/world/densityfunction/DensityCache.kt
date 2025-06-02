package org.basaltmc.world.densityfunction

import org.basaltmc.common.Mth

/**
 * // parity: NoiseChunk's `FlatCache` and `Cache2D`. Vanilla rewrites the router tree into a fresh
 * `NoiseChunk` per chunk; the router here is immutable and shared, so the mutable state lives in
 * this object and travels down inside [FunctionContext.cache]. One instance, one chunk, one
 * thread, so no lock.
 *
 * [flatCache] is not memoization: vanilla quantizes to the quart grid and samples at `y = 0`, and
 * reproducing that quantization is the point.
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
    private val interpolatedValues = arrayOfNulls<MutableMap<Long, Double>>(slotCount)
    private var interpolationCellWidth = 0
    private var interpolationCellHeight = 0

    /** Supplies the noise-cell dimensions used by vanilla's `NoiseChunk.Interpolator`. */
    public fun configureInterpolation(
        cellWidth: Int,
        cellHeight: Int,
    ) {
        require(cellWidth > 0 && cellHeight > 0) { "Interpolation cell dimensions must be positive" }
        if (interpolationCellWidth == 0) {
            interpolationCellWidth = cellWidth
            interpolationCellHeight = cellHeight
        } else {
            require(interpolationCellWidth == cellWidth && interpolationCellHeight == cellHeight) {
                "Density cache already uses ${interpolationCellWidth}x$interpolationCellHeight cells, " +
                    "cannot reconfigure it to ${cellWidth}x$cellHeight"
            }
        }
    }

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

    /**
     * // parity: `NoiseChunk.Interpolator`. Only [function] is sampled at noise-cell corners;
     * callers continue evaluating every node outside its marker at the real block.
     */
    public fun interpolated(
        slot: Int,
        context: FunctionContext,
        function: DensityFunction,
    ): Double {
        val cellWidth = interpolationCellWidth
        val cellHeight = interpolationCellHeight
        if (slot >= interpolatedValues.size || cellWidth == 0 || cellHeight == 0) {
            return function.compute(context)
        }

        val minX = context.blockX.floorDiv(cellWidth) * cellWidth
        val minY = context.blockY.floorDiv(cellHeight) * cellHeight
        val minZ = context.blockZ.floorDiv(cellWidth) * cellWidth
        val values = interpolatedValues[slot] ?: HashMap<Long, Double>().also { interpolatedValues[slot] = it }

        fun corner(
            x: Int,
            y: Int,
            z: Int,
        ): Double =
            values.getOrPut(packBlockPos(x, y, z)) {
                function.compute(FunctionContext(x, y, z, this))
            }

        val fractionX = (context.blockX - minX).toDouble() / cellWidth
        val fractionY = (context.blockY - minY).toDouble() / cellHeight
        val fractionZ = (context.blockZ - minZ).toDouble() / cellWidth
        return Mth.lerp3(
            fractionX,
            fractionY,
            fractionZ,
            corner(minX, minY, minZ),
            corner(minX + cellWidth, minY, minZ),
            corner(minX, minY + cellHeight, minZ),
            corner(minX + cellWidth, minY + cellHeight, minZ),
            corner(minX, minY, minZ + cellWidth),
            corner(minX + cellWidth, minY, minZ + cellWidth),
            corner(minX, minY + cellHeight, minZ + cellWidth),
            corner(minX + cellWidth, minY + cellHeight, minZ + cellWidth),
        )
    }

    private companion object {
        const val QUARTS_PER_CHUNK = 4

        // parity: NoiseChunk's own `noiseSizeXZ + 1`, with the real Overworld cellWidth of 4 (which
        // makes the noise grid and the quart grid the same thing).
        const val GRID_SIZE = QUARTS_PER_CHUNK + 1

        // Same 26/12/26 bit layout as BlockPos. Every legal dimension Y fits in twelve bits.
        fun packBlockPos(
            x: Int,
            y: Int,
            z: Int,
        ): Long =
            ((x.toLong() and 0x3FFFFFFL) shl 38) or
                ((z.toLong() and 0x3FFFFFFL) shl 12) or
                (y.toLong() and 0xFFFL)
    }
}
