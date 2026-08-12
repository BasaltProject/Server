package org.basaltmc.server.world.densityfunction

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.world.noise.NormalNoise
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * No jar cross-check of `compute` here: vanilla's `NoiseHolder` only resolves at world load, so a
 * harness constructing one directly always samples zero. The scale factors and coordinate order
 * still come from the decompiled `DensityFunctions.ShiftNoise`/`Shift`/`ShiftA`/`ShiftB`.
 *
 * The bounds do not depend on that resolution and were cross-checked: `-8.0`/`8.0` for a noise
 * whose own `maxValue()` is `2.0`, matching `offsetNoise.maxValue * 4.0`.
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
