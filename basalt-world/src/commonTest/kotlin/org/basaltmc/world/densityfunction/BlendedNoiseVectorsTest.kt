package org.basaltmc.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

/** Golden vectors captured by running the real decompiled 26.2 `BlendedNoise` bytecode. */
class BlendedNoiseVectorsTest {
    @Test
    fun overworldMatchesVanilla() {
        val overworld = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)
        assertEquals(0.05283727086562935, overworld.compute(FunctionContext(0, 0, 0)), 1.0E-9)
        assertEquals(0.12348833259699732, overworld.compute(FunctionContext(10, -20, 5)), 1.0E-9)
        assertEquals(0.4324598817684787, overworld.compute(FunctionContext(-100, 64, 200)), 1.0E-9)
        assertEquals(-0.007408630475327471, overworld.compute(FunctionContext(64, 0, -64)), 1.0E-9)
        assertEquals(0.2488408853630668, overworld.compute(FunctionContext(1000, -32, 1000)), 1.0E-9)
        assertEquals(-87.55150000000002, overworld.minValue(), 1.0E-9)
        assertEquals(87.55150000000002, overworld.maxValue(), 1.0E-9)
    }

    @Test
    fun netherMatchesVanilla() {
        val nether = BlendedNoise.createUnseeded(0.25, 0.375, 80.0, 60.0, 8.0)
        assertEquals(0.05283727086562935, nether.compute(FunctionContext(0, 0, 0)), 1.0E-9)
        assertEquals(-0.12200273172480704, nether.compute(FunctionContext(50, 10, -50)), 1.0E-9)
    }

    @Test
    fun endMatchesVanilla() {
        val end = BlendedNoise.createUnseeded(0.25, 0.25, 80.0, 160.0, 4.0)
        assertEquals(0.05283727086562935, end.compute(FunctionContext(0, 0, 0)), 1.0E-9)
        assertEquals(-0.11888597357872246, end.compute(FunctionContext(30, 30, 30)), 1.0E-9)
    }
}
