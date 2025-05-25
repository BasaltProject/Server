package org.basaltmc.world.terrain

import org.basaltmc.common.Mth
import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext

/**
 * // parity: net.minecraft.world.level.levelgen.NoiseChunk (the per-chunk cell-interpolation
 * scheme only, not its incremental slope-tracking optimization, which produces the exact same
 * numeric result faster; not needed for correctness, only for shaving further off this class's
 * own already-real speedup). Real vanilla never evaluates a density function tree per block: it
 * samples [density] only at the corners of a coarse 3D cell grid ([cellWidth] blocks apart
 * horizontally, [cellHeight] apart vertically, both confirmed from the real 26.2
 * `NoiseSettings.OVERWORLD_NOISE_SETTINGS`) and trilinearly interpolates every block inside a
 * cell from its 8 corners. This is not an approximation of some "true" per-block density: the
 * interpolated grid *is* what vanilla actually generates, so this class's output is exact, not a
 * performance shortcut with a fidelity cost, unlike the coarse sampling
 * [org.basaltmc.world.NoiseWorldGenerator] used before real parity work started on this.
 *
 * One instance samples exactly one chunk column range: [minY]..[minY] + [height] - 1, in world
 * coordinates `chunkX * 16 + localX` / `chunkZ * 16 + localZ`. [cellWidth] must evenly divide 16
 * and [cellHeight] must evenly divide [height]; both hold for the real Overworld settings
 * (`cellWidth = 4`, `cellHeight = 8`, `height = 384`).
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
