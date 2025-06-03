package org.basaltmc.server.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

class ConstantTest {
    @Test
    fun ignoresPositionAndBoundsItself() {
        val constant = Constant(2.5)
        assertEquals(2.5, constant.compute(FunctionContext(0, 0, 0)))
        assertEquals(2.5, constant.compute(FunctionContext(100, -50, 30)))
        assertEquals(2.5, constant.minValue())
        assertEquals(2.5, constant.maxValue())
    }
}
