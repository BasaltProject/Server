package org.basaltmc.server.runtime.world

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.PositionalRandomFactory
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.terrain.NetherBiomeSource
import org.basaltmc.server.world.terrain.NetherNoiseRouter
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.concurrent.Volatile

/** Real named noises, random streams and biomes used by Nether surface rules. */
public class NetherSurfaceEnvironment(
    serverJar: ServerJarAssets,
    private val random: PositionalRandomFactory,
    private val router: NetherNoiseRouter,
    private val biomeSource: NetherBiomeSource,
) : SurfaceRuleEnvironment {
    private val noises = RealNoiseParameters(serverJar, random)

    @Volatile
    private var gradientRandoms: Map<String, PositionalRandomFactory> = emptyMap()

    override fun surfaceDepthNoise(
        x: Int,
        z: Int,
    ): Int {
        val noiseValue = noises.get("surface").getValue(x.toDouble(), 0.0, z.toDouble())
        return (noiseValue * 2.75 + 3.0 + random.at(x, 0, z).nextDouble() * 0.25).toInt()
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

    override fun sample3d(
        noiseId: String,
        x: Int,
        y: Int,
        z: Int,
    ): Double = noises.get(noiseId).getValue(x.toDouble(), y.toDouble(), z.toDouble())

    override fun verticalGradientRandom(
        name: String,
        x: Int,
        y: Int,
        z: Int,
    ): Float {
        val factory =
            gradientRandoms[name] ?: random.fromHashOf("minecraft:$name").forkPositional().also {
                gradientRandoms = gradientRandoms + (name to it)
            }
        return factory.at(x, y, z).nextFloat()
    }

    override fun positionalRandom(
        x: Int,
        y: Int,
        z: Int,
    ): BasaltRandom = random.at(x, y, z)

    override fun estimatedSurfaceHeight(
        x: Int,
        z: Int,
    ): Int {
        var y = 127
        while (y >= 0 && router.finalDensity.compute(FunctionContext(x, y, z)) <= 0.0) y--
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
}
