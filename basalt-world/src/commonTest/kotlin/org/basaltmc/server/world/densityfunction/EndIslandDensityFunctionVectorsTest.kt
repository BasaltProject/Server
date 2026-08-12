package org.basaltmc.server.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

/** Golden vectors captured by running the real decompiled 26.2 `DensityFunctions.EndIslandDensityFunction` bytecode. */
class EndIslandDensityFunctionVectorsTest {
    @Test
    fun matchesVanilla() {
        val end = EndIslandDensityFunction(12345L)
        assertEquals(0.5625, end.compute(FunctionContext(0, 0, 0)))
        assertEquals(0.21875, end.compute(FunctionContext(64, 0, 0)))
        assertEquals(0.21875, end.compute(FunctionContext(0, 0, 64)))
        assertEquals(-0.84375, end.compute(FunctionContext(-500, 0, 300)))
        assertEquals(-0.605462908744812, end.compute(FunctionContext(1000, 0, -1000)), 1.0E-9)
        assertEquals(0.5625, end.compute(FunctionContext(8, 0, 8)))
        assertEquals(0.5625, end.compute(FunctionContext(-8, 0, -8)))
        assertEquals(-0.84375, end.compute(FunctionContext(200, 0, 200)))
        assertEquals(-0.84375, end.minValue())
        assertEquals(0.5625, end.maxValue())

        val end2 = EndIslandDensityFunction(999L)
        assertEquals(0.5625, end2.compute(FunctionContext(0, 0, 0)))
    }
}
