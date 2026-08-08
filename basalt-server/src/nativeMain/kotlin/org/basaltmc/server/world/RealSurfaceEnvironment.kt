package org.basaltmc.server.world

import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.PositionalRandomFactory
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.terrain.OverworldBiomeSource
import org.basaltmc.world.terrain.OverworldNoiseRouter
import org.basaltmc.world.terrain.surface.BadlandsClayBands
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.concurrent.Volatile

/**
 * The real [SurfaceRuleEnvironment]: real noise parameters from the downloaded [serverJar] (via
 * [RealNoiseParameters], already built for the density router), the real per-world [random], and
 * [router]'s own `finalDensity` for [estimatedSurfaceHeight] (see that method's own KDoc for why
 * scanning the real density function directly, not reading an already-filled chunk, is the right
 * substitute here). [biomeAt] resolves the real vanilla biome via [OverworldBiomeSource] (the real
 * climate transitioner, [org.basaltmc.world.climate.OverworldBiomeBuilder]), not a fixed biome.
 *
 * **Documented gap**: [isSteep] always reports `false` (real vanilla clamps its neighbor lookup to
 * the specific chunk under construction, which a single reusable environment object spanning every
 * chunk has no notion of); [RealTerrainGenerator][org.basaltmc.world.terrain.RealTerrainGenerator]
 * wraps this environment with a chunk-scoped one that does implement it correctly.
 */
public class RealSurfaceEnvironment(
    serverJar: ServerJarAssets,
    private val random: PositionalRandomFactory,
    private val router: OverworldNoiseRouter,
) : SurfaceRuleEnvironment {
    private val biomeSource = OverworldBiomeSource(router)
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
     * The factory per name is built once, not per block.
     *
     * Surface rules ask for this for essentially every block near the surface, and each call used
     * to concatenate a string, MD5 it, and build two generators, all to arrive at something that
     * depends only on [name] and on a handful of fixed names at that. A profile put a tenth of a
     * whole session's allocation in this one line.
     *
     * Copy-on-write, because a single environment is shared by every generation worker: readers
     * see a fully built immutable map or the previous one, and never a map mid-update. Two threads
     * racing to add the same name both derive the same factory, so whichever lands is equivalent.
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
     * top-down for the first solid Y, exactly what [org.basaltmc.world.Chunk.heightmap] itself
     * does against the already-filled chunk, just recomputed directly from the density function so
     * it works before any chunk exists to read from. Correct but comparatively expensive (a raw,
     * uncached scan of the same tree [org.basaltmc.world.terrain.DensityChunkSampler] exists to
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
    ): String = biomeSource.biomeAt(x, y, z).id.toString()

    override fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache?,
    ): String = biomeSource.biomeAt(x, y, z, cache).id.toString()

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
