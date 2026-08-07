package org.basaltmc.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

class MarkerTest {
    @Test
    fun mostVariantsPassThroughTheWrappedFunctionUnchanged() {
        val wrapped = YClampedGradient(-64, 64, -3.0, 3.0)
        for (type in MarkerType.entries - MarkerType.BLEND_DENSITY) {
            val marker = Marker(type, wrapped)
            assertEquals(wrapped.compute(FunctionContext(0, 12, 0)), marker.compute(FunctionContext(0, 12, 0)))
            assertEquals(wrapped.minValue(), marker.minValue())
            assertEquals(wrapped.maxValue(), marker.maxValue())
        }
    }

    @Test
    fun blendDensityKeepsTheValueButUnboundsTheRange() {
        val wrapped = Constant(1.0)
        val marker = Marker(MarkerType.BLEND_DENSITY, wrapped)
        assertEquals(1.0, marker.compute(FunctionContext(0, 0, 0)))
        assertEquals(Double.NEGATIVE_INFINITY, marker.minValue())
        assertEquals(Double.POSITIVE_INFINITY, marker.maxValue())
    }
}
