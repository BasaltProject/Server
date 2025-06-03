package org.basaltmc.server.world.terrain.surface

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

    /** // parity: Context.getMinSurfaceLevel, folded together with AbovePreliminarySurfaceCondition's own use of it. */
    public fun isAbovePreliminarySurface(): Boolean = blockY >= estimatedSurfaceHeight + surfaceDepth - 8

    public fun sample2d(noiseId: String): Double = environment.sample2d(noiseId, blockX, blockZ)

    public fun sample3d(noiseId: String): Double = environment.sample3d(noiseId, blockX, blockY, blockZ)

    public fun verticalGradientRandom(name: String): Float =
        environment.verticalGradientRandom(name, blockX, blockY, blockZ)

    public fun clayBand(): Int = environment.clayBand(blockX, blockY, blockZ)
}
