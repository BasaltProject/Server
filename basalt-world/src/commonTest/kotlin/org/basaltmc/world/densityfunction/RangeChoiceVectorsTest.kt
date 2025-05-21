package org.basaltmc.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

/** Golden vectors captured by running the real decompiled 26.2 `DensityFunctions.RangeChoice` bytecode. */
class RangeChoiceVectorsTest {
    @Test
    fun matchesVanilla() {
        val input = YClampedGradient(-64, 64, -10.0, 10.0)
        val whenIn = Constant(1.0)
        val whenOut = Constant(-1.0)
        val range = RangeChoice(input, -2.0, 2.0, whenIn, whenOut)

        assertEquals(1.0, range.compute(FunctionContext(0, 0, 0)))
        assertEquals(-1.0, range.compute(FunctionContext(0, 64, 0)))
        assertEquals(-1.0, range.compute(FunctionContext(0, -64, 0)))
        assertEquals(-1.0, range.minValue())
        assertEquals(1.0, range.maxValue())
    }
}
