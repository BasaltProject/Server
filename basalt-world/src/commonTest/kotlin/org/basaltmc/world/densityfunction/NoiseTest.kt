package org.basaltmc.world.densityfunction

import org.basaltmc.random.LegacyRandom
import org.basaltmc.world.noise.NormalNoise
import kotlin.test.Test
import kotlin.test.assertEquals

/** [org.basaltmc.world.noise.NormalNoise] itself is already verified bit-exact elsewhere; this only checks the scaling/bounds wiring. */
class NoiseTest {
    @Test
    fun scalesCoordinatesAndDelegatesToTheWrappedNoise() {
        val random = LegacyRandom(7L)
        val normalNoise = NormalNoise.create(random, -3, doubleArrayOf(1.0, 1.0, 1.0, 1.0))
        val noise = Noise(normalNoise, xzScale = 2.0, yScale = 0.5)

        val expected = normalNoise.getValue(20.0, -8.0, 40.0)
        assertEquals(expected, noise.compute(FunctionContext(10, -16, 20)))
        assertEquals(normalNoise.maxValue, noise.maxValue())
        assertEquals(-normalNoise.maxValue, noise.minValue())
    }
}
