package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/**
 * An overworld router and environment built from synthetic noise, for the tests that measure what
 * generation costs. [NoiseRouterData.overworld] builds the identical tree either way, so the shape
 * of the work is the real one and nothing has to be downloaded.
 */
class SyntheticOverworld {
    val router: OverworldNoiseRouter = NoiseRouterData.overworld(::syntheticNoise, BASE_3D)

    private val biomeSource = OverworldBiomeSource(router)

    /** Real climate resolution for [SurfaceRuleEnvironment.biomeAt], constant stand-ins elsewhere. */
    val environment: SurfaceRuleEnvironment =
        object : SurfaceRuleEnvironment {
            override fun surfaceDepthNoise(
                x: Int,
                z: Int,
            ): Int = 3

            override fun surfaceSecondaryNoise(
                x: Int,
                z: Int,
            ): Double = 0.0

            override fun sample2d(
                noiseId: String,
                x: Int,
                z: Int,
            ): Double = 0.0

            override fun sample3d(
                noiseId: String,
                x: Int,
                y: Int,
                z: Int,
            ): Double = 0.0

            override fun verticalGradientRandom(
                name: String,
                x: Int,
                y: Int,
                z: Int,
            ): Float = 1.0f

            override fun positionalRandom(
                x: Int,
                y: Int,
                z: Int,
            ): BasaltRandom = XoroshiroRandom(0L)

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
        }

    private companion object {
        val BASE_3D = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

        fun syntheticNoise(name: String): NormalNoise =
            NormalNoise.create(XoroshiroRandom(name.hashCode().toLong()), -5, doubleArrayOf(1.0, 1.0, 1.0, 1.0, 1.0))
    }
}
