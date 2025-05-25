package org.basaltmc.world.terrain

import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.world.GenerationMetrics
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.densityfunction.DensityCache
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.noise.NormalNoise
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * Measures what one chunk actually costs, so the cache-marker and biome-lookup work has a number
 * to move rather than an intuition. Synthetic noise rather than the real downloaded parameters
 * (same approach as [RealTerrainGeneratorTest]): [NoiseRouterData.overworld] builds the identical
 * tree either way, and the counts asserted here follow from that tree's shape, not from the
 * parameter values, so this runs offline and in CI like any other test.
 *
 * The assertions are deliberately written as "this is still unnecessarily expensive" bounds. They
 * are expected to fail, loudly and on purpose, once [org.basaltmc.world.densityfunction.Marker]
 * caches for real, at which point they get tightened to the new, lower numbers.
 */
class GenerationCostTest {
    private fun syntheticNoise(name: String): NormalNoise {
        val seed = name.hashCode().toLong()
        return NormalNoise.create(XoroshiroRandom(seed), -5, doubleArrayOf(1.0, 1.0, 1.0, 1.0, 1.0))
    }

    private val base3d = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)
    private val router = NoiseRouterData.overworld(::syntheticNoise, base3d)
    private val biomeSource = OverworldBiomeSource(router)

    /** Real climate resolution for [biomeAt], constant stand-ins everywhere else: the biome path is what this measures. */
    private val environment =
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

    @AfterTest
    fun tearDown() {
        GenerationMetrics.enabled = false
        GenerationMetrics.reset()
    }

    @Test
    fun oneChunkCostsWhatTheCacheMarkersWouldOtherwiseSave() {
        GenerationMetrics.reset()
        GenerationMetrics.enabled = true
        RealTerrainGenerator.generate(router, environment, 0, 0)
        GenerationMetrics.enabled = false

        val report = GenerationMetrics.report()
        println(report)

        // fillBiomes alone walks every quart cell of the column: 96 vertical quarts times 4x4.
        assertTrue(
            "Biome lookups            1536" in report,
            "expected 1536 biome lookups per chunk, got:\n$report",
        )
        // A ceiling, not a target: this was 105786 before Marker honored FLAT_CACHE/CACHE_2D, and
        // regressing past it means a cache stopped being reached, not that generation got slower.
        val noiseEvaluations = valueAfter(report, "NormalNoise evaluations")
        assertTrue(
            noiseEvaluations in 1..8000,
            "expected the flat caches to hold noise evaluations well under 8000, got $noiseEvaluations:\n$report",
        )
    }

    private fun valueAfter(
        report: String,
        label: String,
    ): Long =
        report
            .lineSequence()
            .first { label in it }
            .substringAfter(label)
            .trim()
            .toLong()
}
