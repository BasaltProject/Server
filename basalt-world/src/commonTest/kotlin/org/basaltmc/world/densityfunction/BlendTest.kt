package org.basaltmc.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

class BlendTest {
    @Test
    fun blendAlphaIsAlwaysOne() {
        assertEquals(1.0, BlendAlpha.compute(FunctionContext(1, 2, 3)))
        assertEquals(1.0, BlendAlpha.minValue())
        assertEquals(1.0, BlendAlpha.maxValue())
    }

    @Test
    fun blendOffsetIsAlwaysZero() {
        assertEquals(0.0, BlendOffset.compute(FunctionContext(1, 2, 3)))
        assertEquals(0.0, BlendOffset.minValue())
        assertEquals(0.0, BlendOffset.maxValue())
    }
}
