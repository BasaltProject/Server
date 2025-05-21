package org.basaltmc.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

class ClampTest {
    @Test
    fun clampsIntoTheDeclaredRange() {
        val clamp = Clamp(Constant(5.0), -1.0, 1.0)
        assertEquals(1.0, clamp.compute(FunctionContext(0, 0, 0)))
        assertEquals(-1.0, Clamp(Constant(-5.0), -1.0, 1.0).compute(FunctionContext(0, 0, 0)))
        assertEquals(0.5, Clamp(Constant(0.5), -1.0, 1.0).compute(FunctionContext(0, 0, 0)))
        assertEquals(-1.0, clamp.minValue())
        assertEquals(1.0, clamp.maxValue())
    }
}
