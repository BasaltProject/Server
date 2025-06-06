package org.basaltmc.server.world.densityfunction

import org.basaltmc.wire.common.Mth

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

    // The cell corners of this chunk, one dense grid per marker. A map keyed by packed position
    // boxed a Double on each of the eight corner lookups every block does, which is most of the
    // work the interpolation exists to avoid.
    private val interpolatedValues = arrayOfNulls<DoubleArray>(slotCount)
    private val interpolatedPresent = arrayOfNulls<BooleanArray>(slotCount)
    private val distantValues = arrayOfNulls<MutableMap<Long, Double>>(slotCount)
    private val lastCellKey = LongArray(slotCount)
    private val lastCellValid = BooleanArray(slotCount)
    private val lastCorners = DoubleArray(slotCount * CORNERS_PER_CELL)
    private val firstBlockX = chunkX * 16
    private val firstBlockZ = chunkZ * 16
    private var interpolationCellWidth = 0
    private var interpolationCellHeight = 0
    private var interpolationMinY = 0
    private var interpolationGridX = 0
    private var interpolationGridY = 0

    /** Supplies the noise-cell dimensions and Y range used by vanilla's `NoiseChunk.Interpolator`. */
    public fun configureInterpolation(
        cellWidth: Int,
        cellHeight: Int,
        minY: Int,
        height: Int,
    ) {
        require(cellWidth > 0 && cellHeight > 0) { "Interpolation cell dimensions must be positive" }
        if (interpolationCellWidth == 0) {
            interpolationCellWidth = cellWidth
            interpolationCellHeight = cellHeight
            interpolationMinY = minY
            interpolationGridX = 16 / cellWidth + 1
            interpolationGridY = height / cellHeight + 1
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

        // Read before the corners are filled: filling them re-enters [function], and the caller's
        // context is reused across blocks rather than rebuilt for each one.
        val fractionX = (context.blockX - minX).toDouble() / cellWidth
        val fractionY = (context.blockY - minY).toDouble() / cellHeight
        val fractionZ = (context.blockZ - minZ).toDouble() / cellWidth

        // The terrain fill walks a whole cell before moving to the next, so all but one of a
        // cell's 128 blocks find the eight corners already in hand and only pay the lerp.
        val key = packBlockPos(minX, minY, minZ)
        val base = slot * CORNERS_PER_CELL
        if (!lastCellValid[slot] || lastCellKey[slot] != key) {
            lastCorners[base] = corner(slot, function, minX, minY, minZ)
            lastCorners[base + 1] = corner(slot, function, minX + cellWidth, minY, minZ)
            lastCorners[base + 2] = corner(slot, function, minX, minY + cellHeight, minZ)
            lastCorners[base + 3] = corner(slot, function, minX + cellWidth, minY + cellHeight, minZ)
            lastCorners[base + 4] = corner(slot, function, minX, minY, minZ + cellWidth)
            lastCorners[base + 5] = corner(slot, function, minX + cellWidth, minY, minZ + cellWidth)
            lastCorners[base + 6] = corner(slot, function, minX, minY + cellHeight, minZ + cellWidth)
            lastCorners[base + 7] = corner(slot, function, minX + cellWidth, minY + cellHeight, minZ + cellWidth)
            lastCellKey[slot] = key
            lastCellValid[slot] = true
        }

        return Mth.lerp3(
            fractionX,
            fractionY,
            fractionZ,
            lastCorners[base],
            lastCorners[base + 1],
            lastCorners[base + 2],
            lastCorners[base + 3],
            lastCorners[base + 4],
            lastCorners[base + 5],
            lastCorners[base + 6],
            lastCorners[base + 7],
        )
    }

    /**
     * One cell corner. Corners of this chunk come off a dense grid; the aquifer also asks for
     * positions well outside it (and, following vanilla, for cell indices used as block
     * coordinates), and those keep the map.
     */
    private fun corner(
        slot: Int,
        function: DensityFunction,
        x: Int,
        y: Int,
        z: Int,
    ): Double {
        val gridX = (x - firstBlockX) / interpolationCellWidth
        val gridY = (y - interpolationMinY) / interpolationCellHeight
        val gridZ = (z - firstBlockZ) / interpolationCellWidth
        if (gridX < 0 ||
            gridX >= interpolationGridX ||
            gridY < 0 ||
            gridY >= interpolationGridY ||
            gridZ < 0 ||
            gridZ >= interpolationGridX
        ) {
            return distantCorner(slot, function, x, y, z)
        }
        val size = interpolationGridX * interpolationGridY * interpolationGridX
        val values = interpolatedValues[slot] ?: DoubleArray(size).also { interpolatedValues[slot] = it }
        val present = interpolatedPresent[slot] ?: BooleanArray(size).also { interpolatedPresent[slot] = it }
        val index = (gridX * interpolationGridY + gridY) * interpolationGridX + gridZ
        if (!present[index]) {
            values[index] = function.compute(FunctionContext(x, y, z, this))
            present[index] = true
        }
        return values[index]
    }

    private fun distantCorner(
        slot: Int,
        function: DensityFunction,
        x: Int,
        y: Int,
        z: Int,
    ): Double {
        val values = distantValues[slot] ?: HashMap<Long, Double>().also { distantValues[slot] = it }
        return values.getOrPut(packBlockPos(x, y, z)) { function.compute(FunctionContext(x, y, z, this)) }
    }

    private companion object {
        const val QUARTS_PER_CHUNK = 4
        const val CORNERS_PER_CELL = 8

        // Same 26/12/26 bit layout as BlockPos. Every legal dimension Y fits in twelve bits.
        fun packBlockPos(
            x: Int,
            y: Int,
            z: Int,
        ): Long =
            ((x.toLong() and 0x3FFFFFFL) shl 38) or
                ((z.toLong() and 0x3FFFFFFL) shl 12) or
                (y.toLong() and 0xFFFL)

        // parity: NoiseChunk's own `noiseSizeXZ + 1`, with the real Overworld cellWidth of 4 (which
        // makes the noise grid and the quart grid the same thing).
        const val GRID_SIZE = QUARTS_PER_CHUNK + 1
    }
}
