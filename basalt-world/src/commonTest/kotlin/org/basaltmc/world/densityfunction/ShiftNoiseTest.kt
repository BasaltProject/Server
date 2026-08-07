package org.basaltmc.world.densityfunction

import org.basaltmc.random.LegacyRandom
import org.basaltmc.world.noise.NormalNoise
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Vanilla's real `NoiseHolder` only resolves to an actual [NormalNoise] at world load (via a
 * registry lookup this project does not build yet); constructed directly, as a real Java harness
 * would have to, it always samples as zero, which makes the harness comparison this project
 * otherwise leans on uninformative for these nodes' `compute` path (see [DensityFunctionVectorsTest]
 * for a case where that comparison is informative). The `0.25`/`4.0` scale factors and the
 * per-node coordinate argument order below are still read directly from the decompiled
 * `net.minecraft.world.level.levelgen.DensityFunctions.ShiftNoise`/`Shift`/`ShiftA`/`ShiftB`
 * source, the same source of truth this project always uses; only the live cross-check step is
 * skipped here. [maxValue]/[minValue] bounds, which do not depend on holder resolution, were
 * cross-checked against the real jar (see the same harness's `shiftA.minValue`/`maxValue` output:
 * `-8.0`/`8.0` for a noise whose own `maxValue()` is `2.0`, matching `offsetNoise.maxValue * 4.0`).
 */
class ShiftNoiseTest {
    private fun noise(): NormalNoise = NormalNoise.create(LegacyRandom(7L), -3, doubleArrayOf(1.0, 1.0, 1.0, 1.0))

    @Test
    fun shiftUsesAllThreeAxesQuarterScaled() {
        val n = noise()
        val expected = n.getValue(37 * 0.25, -12 * 0.25, -204 * 0.25) * 4.0
        assertEquals(expected, Shift(n).compute(FunctionContext(37, -12, -204)))
    }

    @Test
    fun shiftAZeroesTheYAxis() {
        val n = noise()
        val expected = n.getValue(37 * 0.25, 0.0, -204 * 0.25) * 4.0
        assertEquals(expected, ShiftA(n).compute(FunctionContext(37, -12, -204)))
    }

    @Test
    fun shiftBPermutesAxesAndZeroesTheThird() {
        val n = noise()
        val expected = n.getValue(-204 * 0.25, 37 * 0.25, 0.0) * 4.0
        assertEquals(expected, ShiftB(n).compute(FunctionContext(37, -12, -204)))
    }

    @Test
    fun boundsAreOffsetNoiseMaxValueTimesFour() {
        val shiftA = ShiftA(noise())
        assertEquals(shiftA.offsetNoise.maxValue * 4.0, shiftA.maxValue())
        assertEquals(-shiftA.maxValue(), shiftA.minValue())
    }
}
