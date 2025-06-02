package org.basaltmc.world.terrain

import org.basaltmc.common.Mth
import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext

/**
 * // parity: net.minecraft.world.level.levelgen.NoiseChunk, its cell-interpolation scheme without
 * the incremental slope tracking, which is the same numbers faster. Vanilla never evaluates a
 * density tree per block: it samples [density] at the corners of a [cellWidth] by [cellHeight]
 * grid and trilinearly interpolates the rest, so that grid *is* the terrain and this output is
 * exact, not an approximation.
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
    private val cache: DensityCache? = null,
) {
    init {
        require(16 % cellWidth == 0) { "cellWidth must evenly divide 16, got $cellWidth" }
        require(height % cellHeight == 0) { "cellHeight must evenly divide height ($height), got $cellHeight" }
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
            density.compute(FunctionContext(worldX, worldY, worldZ, cache))
        }
    }

    /** Forces the corner grid now, so callers can time grid construction apart from interpolation. */
    public fun prepare() {
        grid
    }

    /**
     * Whether every one of a cell's eight corners is non-positive, in which case no block inside it
     * can be solid: trilinear interpolation of eight values that are all `<= 0` is itself `<= 0`.
     * Lets a caller skip whole 4x8x4 cells of empty sky instead of interpolating each block only to
     * write nothing. Coordinates are cell indices, not blocks.
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

        val cellX = localX / cellWidth
        val cellY = relativeY / cellHeight
        val cellZ = localZ / cellWidth
        val fracX = (localX % cellWidth).toDouble() / cellWidth
        val fracY = (relativeY % cellHeight).toDouble() / cellHeight
        val fracZ = (localZ % cellWidth).toDouble() / cellWidth

        return Mth.lerp3(
            fracX,
            fracY,
            fracZ,
            cornerValue(cellX, cellY, cellZ),
            cornerValue(cellX + 1, cellY, cellZ),
            cornerValue(cellX, cellY + 1, cellZ),
            cornerValue(cellX + 1, cellY + 1, cellZ),
            cornerValue(cellX, cellY, cellZ + 1),
            cornerValue(cellX + 1, cellY, cellZ + 1),
            cornerValue(cellX, cellY + 1, cellZ + 1),
            cornerValue(cellX + 1, cellY + 1, cellZ + 1),
        )
    }
}
