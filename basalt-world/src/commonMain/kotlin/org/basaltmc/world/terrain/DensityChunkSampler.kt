package org.basaltmc.world.terrain

import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.densityfunction.Marker

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
        evaluationCache.configureInterpolation(cellWidth, cellHeight)
    }

    private val cellsPerAxis = 16 / cellWidth
    private val cellsVertical = height / cellHeight
    private val gridSizeX = cellsPerAxis + 1
    private val gridSizeY = cellsVertical + 1
    private val gridSizeZ = cellsPerAxis + 1

    private val grid: DoubleArray by lazy {
        DoubleArray(gridSizeX * gridSizeY * gridSizeZ) { index ->
            val gx = index / (gridSizeY * gridSizeZ)
            val gy = (index / gridSizeZ) % gridSizeY
            val gz = index % gridSizeZ
            val worldX = chunkX * 16 + gx * cellWidth
            val worldY = minY + gy * cellHeight
            val worldZ = chunkZ * 16 + gz * cellWidth
            density.compute(FunctionContext(worldX, worldY, worldZ, evaluationCache))
        }
    }

    /** Prewarms final-density corners and the interpolation-marker corners reached from them. */
    public fun prepare() {
        grid
    }

    /**
     * Whether every final-density corner of a cell is non-positive. This is diagnostic only: nodes
     * outside interpolation markers may make an interior value positive, so generation must not
     * use this result to skip the cell.
     */
    public fun cellIsAllNonPositive(
        cellX: Int,
        cellY: Int,
        cellZ: Int,
    ): Boolean {
        for (dx in 0..1) {
            for (dy in 0..1) {
                for (dz in 0..1) {
                    if (cornerValue(cellX + dx, cellY + dy, cellZ + dz) > 0.0) return false
                }
            }
        }
        return true
    }

    /** Cell counts, so a caller can iterate the same grid this samples from. */
    public val horizontalCells: Int get() = cellsPerAxis

    public val verticalCells: Int get() = cellsVertical

    private fun cornerValue(
        gx: Int,
        gy: Int,
        gz: Int,
    ): Double = grid[gx * (gridSizeY * gridSizeZ) + gy * gridSizeZ + gz]

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

        val worldX = chunkX * 16 + localX
        val worldZ = chunkZ * 16 + localZ
        return density.compute(FunctionContext(worldX, y, worldZ, evaluationCache))
    }
}
