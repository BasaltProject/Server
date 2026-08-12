package org.basaltmc.server.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

/** Golden vectors captured by running the real decompiled 26.2 `DensityFunctions.FindTopSurface` bytecode. */
class FindTopSurfaceVectorsTest {
    private val density = YClampedGradient(-64, 100, 5.0, -5.0)

    @Test
    fun matchesVanilla() {
        val find = FindTopSurface(density, Constant(64.0), lowerBound = -64, cellHeight = 4)
        assertEquals(16.0, find.compute(FunctionContext(0, 0, 0)))
        assertEquals(-64.0, find.minValue())
        assertEquals(64.0, find.maxValue())
    }

    @Test
    fun shortCircuitsWhenUpperBoundIsAtOrBelowLowerBound() {
        val find = FindTopSurface(density, Constant(-100.0), lowerBound = -64, cellHeight = 4)
        assertEquals(-64.0, find.compute(FunctionContext(0, 0, 0)))
    }

    @Test
    fun fallsBackToLowerBoundWhenNeverPositive() {
        val find = FindTopSurface(Constant(-1.0), Constant(64.0), lowerBound = -64, cellHeight = 4)
        assertEquals(-64.0, find.compute(FunctionContext(0, 0, 0)))
    }
}
