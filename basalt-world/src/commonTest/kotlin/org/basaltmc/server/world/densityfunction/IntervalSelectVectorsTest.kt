package org.basaltmc.server.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

/** Golden vectors captured by running the real decompiled 26.2 `DensityFunctions.IntervalSelect` bytecode. */
class IntervalSelectVectorsTest {
    private fun select(): IntervalSelect =
        IntervalSelect(
            input = YClampedGradient(0, 100, 0.0, 10.0),
            thresholds = doubleArrayOf(3.0, 7.0),
            functions = listOf(Constant(-1.0), Constant(0.0), Constant(1.0)),
        )

    @Test
    fun matchesVanilla() {
        val s = select()
        assertEquals(-1.0, s.compute(FunctionContext(0, 0, 0)))
        assertEquals(-1.0, s.compute(FunctionContext(0, 29, 0)))
        assertEquals(0.0, s.compute(FunctionContext(0, 30, 0)))
        assertEquals(0.0, s.compute(FunctionContext(0, 69, 0)))
        assertEquals(1.0, s.compute(FunctionContext(0, 70, 0)))
        assertEquals(1.0, s.compute(FunctionContext(0, 100, 0)))
        assertEquals(-1.0, s.minValue())
        assertEquals(1.0, s.maxValue())
    }

    @Test
    fun wrongThresholdCountThrows() {
        assertFailsWith<IllegalArgumentException> {
            IntervalSelect(Constant(0.0), doubleArrayOf(1.0), listOf(Constant(0.0), Constant(1.0), Constant(2.0)))
        }
    }

    @Test
    fun unorderedThresholdsThrow() {
        assertFailsWith<IllegalArgumentException> {
            IntervalSelect(Constant(0.0), doubleArrayOf(5.0, 1.0), listOf(Constant(0.0), Constant(1.0), Constant(2.0)))
        }
    }
}
