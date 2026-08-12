package org.basaltmc.server.world.densityfunction

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.world.noise.NormalNoise
import kotlin.test.Test
import kotlin.test.assertEquals

/** Same holder-resolution caveat as [ShiftNoiseTest]: the coordinate formula is verified directly, `maxValue`/`minValue` against the real jar. */
class ShiftedNoiseTest {
    @Test
    fun addsTheShiftFunctionsToScaledCoordinates() {
        val n = NormalNoise.create(LegacyRandom(7L), -3, doubleArrayOf(1.0, 1.0, 1.0, 1.0))
        val shiftX = Constant(1.5)
        val shiftY = Constant(-0.5)
        val shiftZ = Constant(2.0)
        val shiftedNoise = ShiftedNoise(shiftX, shiftY, shiftZ, xzScale = 0.5, yScale = 0.25, noise = n)

        val context = FunctionContext(10, -20, 30)
        val expectedX = 10 * 0.5 + 1.5
        val expectedY = -20 * 0.25 + -0.5
        val expectedZ = 30 * 0.5 + 2.0
        assertEquals(n.getValue(expectedX, expectedY, expectedZ), shiftedNoise.compute(context))
        assertEquals(n.maxValue, shiftedNoise.maxValue())
        assertEquals(-n.maxValue, shiftedNoise.minValue())
    }
}
