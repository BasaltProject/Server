package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.PositionalRandomFactory
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.terrain.OverworldBiomeSource
import org.basaltmc.server.world.terrain.OverworldNoiseRouter
import org.basaltmc.server.world.terrain.surface.BadlandsClayBands
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.concurrent.Volatile

/**
 * The real [SurfaceRuleEnvironment]: noise parameters from the downloaded [serverJar], the
 * per-world [random], [router]'s `finalDensity` for [estimatedSurfaceHeight], and [biomeAt]
 * through [OverworldBiomeSource] rather than a fixed biome.
 *
 * [isSteep] always reports `false` here, since vanilla clamps its neighbour lookup to the chunk
 * under construction and this object spans every chunk. `RealTerrainGenerator` wraps it in a
 * chunk-scoped environment that implements it.
 */
public class RealSurfaceEnvironment(
    serverJar: ServerJarAssets,
    private val random: PositionalRandomFactory,
    private val router: OverworldNoiseRouter,
    override val biomeZoomSeed: Long,
) : SurfaceRuleEnvironment {
    private val biomeSource = OverworldBiomeSource(router, biomeZoomSeed)
    private val noises = RealNoiseParameters(serverJar, random)
    private val clayBands = BadlandsClayBands(random.fromHashOf("minecraft:clay_bands"))

    @Volatile
    private var gradientRandoms: Map<String, PositionalRandomFactory> = emptyMap()

    override fun surfaceDepthNoise(
        x: Int,
        z: Int,
    ): Int {
        val noiseValue = noises.get("surface").getValue(x.toDouble(), 0.0, z.toDouble())
        val jitter = random.at(x, 0, z).nextDouble()
        return (noiseValue * 2.75 + 3.0 + jitter * 0.25).toInt()
    }

    override fun surfaceSecondaryNoise(
        x: Int,
        z: Int,
    ): Double = noises.get("surface_secondary").getValue(x.toDouble(), 0.0, z.toDouble())

    override fun sample2d(
        noiseId: String,
        x: Int,
        z: Int,
    ): Double = noises.get(noiseId).getValue(x.toDouble(), 0.0, z.toDouble())

    override fun sample2dScaled(
        noiseId: String,
        x: Double,
        z: Double,
    ): Double = noises.get(noiseId).getValue(x, 0.0, z)

    override fun sample3d(
        noiseId: String,
        x: Int,
        y: Int,
        z: Int,
    ): Double = noises.get(noiseId).getValue(x.toDouble(), y.toDouble(), z.toDouble())

    /**
     * One factory per name, not per block: surface rules ask for nearly every block near the
     * surface, and each call concatenated a string, MD5'd it and built two generators for something
     * that depends only on [name]. A profile put a tenth of a session's allocation on this line.
     *
     * Copy-on-write, since every generation worker shares one environment: a reader sees a fully
     * built map or the previous one. Racing threads derive the same factory anyway.
     */
    override fun verticalGradientRandom(
        name: String,
        x: Int,
        y: Int,
        z: Int,
    ): Float {
        val known = gradientRandoms[name]
        val factory =
            known ?: random.fromHashOf("minecraft:$name").forkPositional().also {
                gradientRandoms = gradientRandoms + (name to it)
            }
        return factory.at(x, y, z).nextFloat()
    }

    override fun positionalRandom(
        x: Int,
        y: Int,
        z: Int,
    ): BasaltRandom = random.at(x, y, z)

    /**
     * // parity: net.minecraft.world.level.levelgen.NoiseChunk.preliminarySurfaceLevel (a
     * deliberate substitute, see this class's own KDoc): scans [router]'s own `finalDensity`
     * top-down for the first solid Y, exactly what [org.basaltmc.server.world.Chunk.heightmap] itself
     * does against the already-filled chunk, just recomputed directly from the density function so
     * it works before any chunk exists to read from. Correct but comparatively expensive (a raw,
     * uncached scan of the same tree [org.basaltmc.server.world.terrain.DensityChunkSampler] exists to
     * avoid re-evaluating this often): `RealTerrainGenerator` wraps this environment with the
     * already-filled chunk's own heightmap before actually generating, so this only runs if
     * something calls `SurfaceBuilder.buildSurface` with this environment directly, unwrapped.
     */
    override fun estimatedSurfaceHeight(
        x: Int,
        z: Int,
    ): Int {
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y && router.finalDensity.compute(FunctionContext(x, y, z)) <= 0.0) y--
        return y + 1
    }

    override fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
    ): String = biomeSource.blockBiomeAt(x, y, z).id.toString()

    override fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache?,
    ): String = biomeSource.blockBiomeAt(x, y, z, cache).id.toString()

    override fun noiseBiomeAt(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
        cache: DensityCache?,
    ): String = biomeSource.biomeAtQuart(quartX, quartY, quartZ, cache).id.toString()

    override fun isSteep(
        x: Int,
        z: Int,
    ): Boolean = false

    override fun clayBand(
        x: Int,
        y: Int,
        z: Int,
    ): Int = clayBands.stateId(y, noises.get("clay_bands_offset").getValue(x.toDouble(), 0.0, z.toDouble()))
}
