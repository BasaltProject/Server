package org.basaltmc.world.terrain

import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.densityfunction.YClampedGradient
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals

class DensityChunkSamplerTest {
    @Test
    fun matchesDirectComputeExactlyAtGridCorners() {
        // A function that genuinely varies in all three axes, so corner reads can't accidentally
        // agree by symmetry.
        val density =
            object : DensityFunction {
                override fun compute(context: FunctionContext): Double =
                    context.blockX * 3.0 + context.blockY * 5.0 + context.blockZ * 7.0

                override fun minValue(): Double = Double.NEGATIVE_INFINITY

                override fun maxValue(): Double = Double.POSITIVE_INFINITY
            }
        val sampler =
            DensityChunkSampler(
                density,
                chunkX = 2,
                chunkZ = -3,
                minY = -64,
                height = 384,
                cellWidth = 4,
                cellHeight = 8,
            )

        // Every grid corner (multiples of cellWidth/cellHeight from the chunk/world origin) has
        // fracX = fracY = fracZ = 0, so trilinear interpolation must reproduce compute() exactly.
        for (localX in intArrayOf(0, 4, 8, 12)) {
            for (localZ in intArrayOf(0, 4, 8, 12)) {
                for (y in intArrayOf(-64, -56, 0, 64, 128, 312)) {
                    val worldX = 2 * 16 + localX
                    val worldZ = -3 * 16 + localZ
                    val expected = density.compute(FunctionContext(worldX, y, worldZ))
                    assertEquals(expected, sampler.sample(localX, y, localZ), "at ($localX, $y, $localZ)")
                }
            }
        }
    }

    @Test
    fun exactlyReproducesALinearFunctionEverywhereNotJustAtCorners() {
        // A function linear in Y (and constant in X/Z): linear interpolation between two points of
        // a linear function has zero error, so this must match the real formula at every Y, not
        // just at cell boundaries.
        val gradient = YClampedGradient(-64, 320, 0.0, 1.0)
        val sampler =
            DensityChunkSampler(
                gradient,
                chunkX = 0,
                chunkZ = 0,
                minY = -64,
                height = 384,
                cellWidth = 4,
                cellHeight = 8,
            )

        for (y in -64 until 320) {
            val expected = gradient.compute(FunctionContext(0, y, 0))
            assertEquals(expected, sampler.sample(8, y, 8), 1e-12, "at y=$y")
        }
    }

    @Test
    fun cachesTheGridAcrossRepeatedSamples() {
        var evaluations = 0
        val density =
            object : DensityFunction {
                override fun compute(context: FunctionContext): Double {
                    evaluations++
                    return context.blockY.toDouble()
                }

                override fun minValue(): Double = Double.NEGATIVE_INFINITY

                override fun maxValue(): Double = Double.POSITIVE_INFINITY
            }
        val sampler =
            DensityChunkSampler(
                density,
                chunkX = 0,
                chunkZ = 0,
                minY = -64,
                height = 384,
                cellWidth = 4,
                cellHeight = 8,
            )

        sampler.sample(0, 0, 0)
        val afterFirst = evaluations
        sampler.sample(5, 10, 3)
        sampler.sample(15, 300, 15)
        assertEquals(afterFirst, evaluations, "the grid should be computed once, not per sample() call")
        // 5 x 49 x 5 grid points for a 16-wide/384-tall chunk at cellWidth=4/cellHeight=8.
        assertEquals(5 * 49 * 5, afterFirst)
    }

    @Test
    fun rejectsOutOfChunkOrOutOfHeightCoordinates() {
        val density = YClampedGradient(-64, 320, 0.0, 1.0)
        val sampler =
            DensityChunkSampler(
                density,
                chunkX = 0,
                chunkZ = 0,
                minY = -64,
                height = 384,
                cellWidth = 4,
                cellHeight = 8,
            )
        var threw = false
        try {
            sampler.sample(16, 0, 0)
        } catch (e: IllegalArgumentException) {
            threw = true
        }
        assertEquals(true, threw)
    }

    @Test
    fun differentChunksSampleDifferentPositions() {
        val density =
            object : DensityFunction {
                override fun compute(context: FunctionContext): Double = context.blockX.toDouble()

                override fun minValue(): Double = Double.NEGATIVE_INFINITY

                override fun maxValue(): Double = Double.POSITIVE_INFINITY
            }
        val samplerA =
            DensityChunkSampler(
                density,
                chunkX = 0,
                chunkZ = 0,
                minY = -64,
                height = 384,
                cellWidth = 4,
                cellHeight = 8,
            )
        val samplerB =
            DensityChunkSampler(
                density,
                chunkX = 1,
                chunkZ = 0,
                minY = -64,
                height = 384,
                cellWidth = 4,
                cellHeight = 8,
            )
        assertNotEquals(samplerA.sample(0, 0, 0), samplerB.sample(0, 0, 0))
    }
}
