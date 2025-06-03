package org.basaltmc.server.world.terrain.surface

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.world.densityfunction.DensityCache

/**
 * Everything [SurfaceRules] node types need from outside `basalt-world` (real noise data, the
 * real per-world seed, biome lookup): kept as an interface, not a direct dependency on
 * `basalt-assets`, matching this project's own established pattern (`NoiseRouterData.overworld`'s
 * `noise: (String) -> NormalNoise` parameter) of keeping `basalt-world` pure and letting
 * `basalt-server` wire in the real, network-sourced implementation.
 */
public interface SurfaceRuleEnvironment {
    /** // parity: net.minecraft.world.level.levelgen.SurfaceSystem.getSurfaceDepth. */
    public fun surfaceDepthNoise(
        x: Int,
        z: Int,
    ): Int

    /** // parity: net.minecraft.world.level.levelgen.SurfaceSystem.getSurfaceSecondary. */
    public fun surfaceSecondaryNoise(
        x: Int,
        z: Int,
    ): Double

    public fun sample2d(
        noiseId: String,
        x: Int,
        z: Int,
    ): Double

    /** Noise lookup at a scaled, non-integral coordinate, used by SurfaceSystem extensions. */
    public fun sample2dScaled(
        noiseId: String,
        x: Double,
        z: Double,
    ): Double = sample2d(noiseId, x.toInt(), z.toInt())

    public fun sample3d(
        noiseId: String,
        x: Int,
        y: Int,
        z: Int,
    ): Double

    /**
     * A uniform `[0, 1)` draw from the named
     * [PositionalRandomFactory][org.basaltmc.server.random.PositionalRandomFactory], for
     * `VerticalGradientConditionSource`.
     */
    public fun verticalGradientRandom(
        name: String,
        x: Int,
        y: Int,
        z: Int,
    ): Float

    /** The world's root positional random at a block coordinate. */
    public fun positionalRandom(
        x: Int,
        y: Int,
        z: Int,
    ): BasaltRandom = error("positional random is required by biome-specific surface extensions")

    /**
     * A stand-in for vanilla's `NoiseChunk.preliminarySurfaceLevel` (an estimate from the density
     * graph alone, computed before any chunk is actually filled): this project runs surface rules
     * *after* the real chunk is already filled from the real density router, so the real generated
     * height is available directly and used here instead, strictly more accurate than an estimate
     * for the same purpose (deciding "close to the surface"). Documented deliberate substitution,
     * not a guess at vanilla's own unimplemented `NoiseChunk.preliminarySurfaceLevel`.
     */
    public fun estimatedSurfaceHeight(
        x: Int,
        z: Int,
    ): Int

    public fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
    ): String

    /**
     * [biomeAt] against one chunk's own [DensityCache], so the climate router's `flatCache` subtrees
     * resolve once per quart column instead of once per query. Defaults to the uncached path for
     * environments with no density functions behind them.
     */
    public fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache?,
    ): String = biomeAt(x, y, z)

    /** // parity: net.minecraft.world.level.levelgen.SurfaceRules.Context.SteepMaterialCondition. */
    public fun isSteep(
        x: Int,
        z: Int,
    ): Boolean

    /** // parity: net.minecraft.world.level.levelgen.SurfaceSystem.getBand. Not implemented: only badlands needs it. */
    public fun clayBand(
        x: Int,
        y: Int,
        z: Int,
    ): Int = throw NotImplementedError("clay bands are not implemented yet (only the badlands biome needs them)")
}
