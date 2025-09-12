package org.basaltmc.server.world.terrain.surface

import org.basaltmc.wire.common.Mth
import kotlin.math.floor

/**
 * // parity: net.minecraft.world.level.levelgen.SurfaceRules.Context. Mutable per-column/per-block
 * state a [SurfaceSystem]-style driver updates while walking a column top-down, that condition/rule
 * node types read from.
 *
 * The real `Context` memoizes several fields behind version counters (`lastUpdateXZ`/`lastUpdateY`)
 * purely as a performance optimization for a value that's read from multiple places in the same
 * tree at the same position; not ported here, since surface generation runs once per world (not a
 * hot path the way density function evaluation is), so recomputing a pure function of the current
 * position a second time costs nothing worth the complexity.
 */
public class SurfaceRuleContext(
    private val environment: SurfaceRuleEnvironment,
    public val worldGenContext: WorldGenerationContext,
) {
    public var blockX: Int = 0
        private set
    public var blockZ: Int = 0
        private set
    public var surfaceDepth: Int = 0
        private set

    // Cached per column, not per block: [estimatedSurfaceHeight] only depends on (x, z), but
    // `abovePreliminarySurface` is checked once per block in the column, and a fresh call would
    // mean re-scanning the density function up to 384 times per block otherwise (real vanilla
    // caches this exact value for the exact same reason, `Context.getMinSurfaceLevel`).
    private var estimatedSurfaceHeight: Int = 0

    /** The four corners of the 16 by 16 cell this column sits in, // parity: preliminarySurfaceCache. */
    private val preliminaryCorners = IntArray(4)

    public var blockY: Int = 0
        private set
    public var waterHeight: Int = Int.MIN_VALUE
        private set
    public var stoneDepthAbove: Int = 0
        private set
    public var stoneDepthBelow: Int = 0
        private set

    /** // parity: Context.updateXZ. */
    public fun updateXZ(
        x: Int,
        z: Int,
    ) {
        blockX = x
        blockZ = z
        surfaceDepth = environment.surfaceDepthNoise(x, z)
        estimatedSurfaceHeight = environment.estimatedSurfaceHeight(x, z)
        val cellX = x shr 4
        val cellZ = z shr 4
        preliminaryCorners[0] = environment.preliminarySurfaceLevel(cellX shl 4, cellZ shl 4)
        preliminaryCorners[1] = environment.preliminarySurfaceLevel((cellX + 1) shl 4, cellZ shl 4)
        preliminaryCorners[2] = environment.preliminarySurfaceLevel(cellX shl 4, (cellZ + 1) shl 4)
        preliminaryCorners[3] = environment.preliminarySurfaceLevel((cellX + 1) shl 4, (cellZ + 1) shl 4)
    }

    /** // parity: Context.updateY. */
    public fun updateY(
        stoneDepthAbove: Int,
        stoneDepthBelow: Int,
        waterHeight: Int,
        y: Int,
    ) {
        this.stoneDepthAbove = stoneDepthAbove
        this.stoneDepthBelow = stoneDepthBelow
        this.waterHeight = waterHeight
        this.blockY = y
    }

    public fun surfaceSecondary(): Double = environment.surfaceSecondaryNoise(blockX, blockZ)

    public fun biome(): String = environment.biomeAt(blockX, blockY, blockZ)

    public fun isHole(): Boolean = surfaceDepth <= 0

    public fun isSteep(): Boolean = environment.isSteep(blockX, blockZ)

    /**
     * // parity: Context.getMinSurfaceLevel. The level is read at the corners of a 16 by 16 cell and
     * interpolated across it, so it is a smooth, coarse floor rather than the column's own surface.
     */
    public fun minSurfaceLevel(): Int {
        val alongX = (blockX and 15) / 16.0
        val alongZ = (blockZ and 15) / 16.0
        val interpolated =
            Mth.lerp2(
                alongX,
                alongZ,
                preliminaryCorners[0].toDouble(),
                preliminaryCorners[1].toDouble(),
                preliminaryCorners[2].toDouble(),
                preliminaryCorners[3].toDouble(),
            )
        return floor(interpolated).toInt() + surfaceDepth - 8
    }

    public fun isAbovePreliminarySurface(): Boolean = blockY >= minSurfaceLevel()

    public fun sample2d(noiseId: String): Double = environment.sample2d(noiseId, blockX, blockZ)

    public fun sample3d(noiseId: String): Double = environment.sample3d(noiseId, blockX, blockY, blockZ)

    public fun verticalGradientRandom(name: String): Float =
        environment.verticalGradientRandom(name, blockX, blockY, blockZ)

    public fun clayBand(): Int = environment.clayBand(blockX, blockY, blockZ)
}
