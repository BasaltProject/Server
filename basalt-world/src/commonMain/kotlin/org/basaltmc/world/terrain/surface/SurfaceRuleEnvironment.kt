package org.basaltmc.world.terrain.surface

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

    public fun sample3d(
        noiseId: String,
        x: Int,
        y: Int,
        z: Int,
    ): Double

    /** A uniform `[0, 1)` draw from the named [PositionalRandomFactory][org.basaltmc.random.PositionalRandomFactory], for `VerticalGradientConditionSource`. */
    public fun verticalGradientRandom(
        name: String,
        x: Int,
        y: Int,
        z: Int,
    ): Float

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
