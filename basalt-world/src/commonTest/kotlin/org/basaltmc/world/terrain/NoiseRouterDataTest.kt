package org.basaltmc.world.terrain

import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.noise.NormalNoise
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

/**
 * Structural/self-consistency checks for [NoiseRouterData.overworld]: the graph *assembly* only,
 * not a byte-exact real-jar comparison. [TerrainProvider], the piece with the most literal
 * constants and the highest risk of a transcription error, is already verified bit-exact
 * elsewhere ([TerrainProviderTest]) against a real jar harness; this class is comparatively
 * mechanical glue over already-verified pieces (the individual [org.basaltmc.world.densityfunction]
 * node types have their own `// parity:` provenance and this project's own established review).
 *
 * A real end-to-end bit-exact check against the jar needs the same per-world-seed resolution real
 * vanilla applies via `NoiseRouter.withNewRandom` (not built yet: [overworldRouter]'s `noise`
 * parameter is exactly the extension point that will plug into once it exists), so it is deferred
 * to that point rather than faked here with a mismatched, ad hoc seeding scheme.
 */
class NoiseRouterDataTest {
    private fun syntheticNoise(name: String): NormalNoise {
        val seed = name.hashCode().toLong()
        return NormalNoise.create(XoroshiroRandom(seed), -5, doubleArrayOf(1.0, 1.0, 1.0, 1.0, 1.0))
    }

    private val base3d = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)

    private val router = NoiseRouterData.overworld(::syntheticNoise, base3d)

    @Test
    fun everySubFunctionIsWired() {
        assertNotEquals(0.0, router.shiftX.maxValue())
        assertNotEquals(0.0, router.shiftZ.maxValue())
        assertTrue(router.continents.maxValue() > router.continents.minValue())
        assertTrue(router.erosion.maxValue() > router.erosion.minValue())
        assertTrue(router.temperature.maxValue() > router.temperature.minValue())
        assertTrue(router.vegetation.maxValue() > router.vegetation.minValue())
    }

    @Test
    fun finalDensityVariesAcrossPositions() {
        val samples =
            (0 until 8).map { i ->
                router.finalDensity.compute(FunctionContext(i * 37, 64, i * -53))
            }
        assertTrue(samples.toSet().size > 1, "expected finalDensity to vary across distinct columns, got $samples")
    }

    @Test
    fun finalDensityIsDeterministicForTheSamePosition() {
        val a = router.finalDensity.compute(FunctionContext(100, 64, -200))
        val b = router.finalDensity.compute(FunctionContext(100, 64, -200))
        assertEquals(a, b)
    }

    @Test
    fun finalDensityStaysWithinItsOwnDeclaredBounds() {
        val min = router.finalDensity.minValue()
        val max = router.finalDensity.maxValue()
        for (i in 0 until 20) {
            val value = router.finalDensity.compute(FunctionContext(i * 91, -32 + i * 20, i * -17))
            assertTrue(value in min..max, "compute() $value outside declared bounds [$min, $max] at sample $i")
        }
    }

    @Test
    fun slidesTowardTheConfiguredEdgeTargetsNearTheWorldLimits() {
        // Near the very bottom of the world, `slide`'s bottom factor pushes hard toward
        // `bottomTarget = 0.1171875` (positive, i.e. solid) regardless of the noise underneath,
        // matching real vanilla's bedrock-adjacent floor.
        val deepValue = router.finalDensity.compute(FunctionContext(0, -63, 0))
        assertTrue(deepValue > 0.0, "expected solid density near the world floor, got $deepValue")
    }

    @Test
    fun netherRouterWiresClimateAndSolidFloorAndRoof() {
        val netherBase3d = BlendedNoise.createUnseeded(0.25, 0.375, 80.0, 60.0, 8.0)
        val nether = NoiseRouterData.nether(::syntheticNoise, netherBase3d)

        assertTrue(nether.temperature.maxValue() > nether.temperature.minValue())
        assertTrue(nether.vegetation.maxValue() > nether.vegetation.minValue())
        assertTrue(nether.finalDensity.compute(FunctionContext(0, 0, 0)) > 0.0)
        assertTrue(nether.finalDensity.compute(FunctionContext(0, 127, 0)) > 0.0)
    }

    @Test
    fun endRouterUsesSeededIslandsForErosionAndDensity() {
        val endBase3d = BlendedNoise.createUnseeded(0.25, 0.25, 80.0, 160.0, 4.0)
        val first = NoiseRouterData.end(endBase3d, 1234L)
        val second = NoiseRouterData.end(endBase3d, 5678L)
        val outer = FunctionContext(1200, 64, -900)

        assertNotEquals(first.erosion.compute(outer), second.erosion.compute(outer))
        assertNotEquals(
            first.finalDensity.compute(FunctionContext(0, 32, 0)),
            first.finalDensity.compute(FunctionContext(0, 64, 0)),
        )
        assertTrue(first.finalDensity.compute(FunctionContext(0, 4, 0)) < 0.0)
    }
}
