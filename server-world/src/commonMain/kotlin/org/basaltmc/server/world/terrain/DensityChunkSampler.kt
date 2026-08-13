package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.densityfunction.Marker

/**
 * // parity: net.minecraft.world.level.levelgen.NoiseChunk. Explicit `INTERPOLATED` markers sample
 * their own wrapped subtrees at [cellWidth] by [cellHeight] corners through [DensityCache]; nodes
 * outside those markers are evaluated at the real block. This distinction matters for nonlinear
 * post-processing such as `squeeze` and for cave `min`/`max` composition.
 *
 * One instance covers one chunk column, [minY] up to [minY] + [height] - 1. [cellWidth] must
 * divide 16 and [cellHeight] must divide [height], as the Overworld settings do (4, 8, 384).
 */
public class DensityChunkSampler(
    private val density: DensityFunction,
    private val chunkX: Int,
    private val chunkZ: Int,
    private val minY: Int,
    private val height: Int,
    private val cellWidth: Int,
    private val cellHeight: Int,
    cache: DensityCache? = null,
) {
    init {
        require(16 % cellWidth == 0) { "cellWidth must evenly divide 16, got $cellWidth" }
        require(height % cellHeight == 0) { "cellHeight must evenly divide height ($height), got $cellHeight" }
    }

    private val evaluationCache = cache ?: DensityCache(chunkX, chunkZ, Marker.slotCount)

    init {
        evaluationCache.configureInterpolation(cellWidth, cellHeight, minY, height)
    }

    private val cellsPerAxis = 16 / cellWidth
    private val cellsVertical = height / cellHeight

    /** Cell counts, so a caller can iterate the same grid this samples from. */
    public val horizontalCells: Int get() = cellsPerAxis

    public val verticalCells: Int get() = cellsVertical

    // One context for the whole chunk. Building one per block was 98304 allocations a chunk on the
    // one path that runs for every block there is.
    private val context = FunctionContext(0, 0, 0, evaluationCache)

    /** [localX]/[localZ] are chunk-local (`0..15`); [y] is a world Y within `[minY, minY + height)`. */
    public fun sample(
        localX: Int,
        y: Int,
        localZ: Int,
    ): Double {
        val relativeY = y - minY
        require(localX in 0..15) { "localX must be within a chunk: $localX" }
        require(localZ in 0..15) { "localZ must be within a chunk: $localZ" }
        require(relativeY in 0 until height) { "y must be within [$minY, ${minY + height}): $y" }

        context.moveTo(chunkX * 16 + localX, y, chunkZ * 16 + localZ)
        return density.compute(context)
    }
}
