package org.basaltmc.world.noise

import org.basaltmc.random.LegacyRandom
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Golden vectors generated locally by running the real, decompiled `PerlinSimplexNoise` from the
 * 26.2 server jar (never committed to this repository) at a handful of coordinates with the exact
 * real seeds/octaves [org.basaltmc.world.terrain.BiomeTemperature] uses (`TEMPERATURE_NOISE`:
 * seed 1234, octaves `[0]`; `FROZEN_TEMPERATURE_NOISE`: seed 3456, octaves `[-2, -1, 0]`), then
 * diffed byte-for-byte against this class's own output. Only numeric values are stored here, no
 * jar content.
 */
class PerlinSimplexNoiseTest {
    @Test
    fun matchesTheRealJarAtASampleOfCoordinates() {
        val temperature = PerlinSimplexNoise(LegacyRandom(1234L), listOf(0))
        val frozen = PerlinSimplexNoise(LegacyRandom(3456L), listOf(-2, -1, 0))
        val xs = doubleArrayOf(0.0, 12.5, -37.25, 100.0, -500.75, 8.125, 333.0)
        val zs = doubleArrayOf(0.0, -8.0, 44.5, -12.25, 1000.0, -0.5, -77.75)
        val expectedTemperature =
            doubleArrayOf(
                0.0, -0.07641525504430638, 0.718654881614431, 0.8449865607379375,
                -0.1515667691373931, 0.2951624419786104, 0.49520855886444215,
            )
        val expectedFrozen =
            doubleArrayOf(
                0.0, -0.04376992009740707, -0.24215671138946523, -0.06287236205638891,
                -0.07978575914177771, 0.3159448449441774, -0.08709533599218736,
            )
        for (i in xs.indices) {
            assertEquals(expectedTemperature[i], temperature.getValue(xs[i], zs[i]), "temperature at index $i")
            assertEquals(expectedFrozen[i], frozen.getValue(xs[i], zs[i]), "frozen at index $i")
        }

        val xs2 = doubleArrayOf(10.0, -3.0, 200.5)
        val zs2 = doubleArrayOf(5.0, 44.0, -9.5)
        val expectedFrozenScaled = doubleArrayOf(0.4854785727425417, -0.4128040443062369, 0.04715321975795093)
        for (i in xs2.indices) {
            assertEquals(expectedFrozenScaled[i], frozen.getValue(xs2[i] * 0.05, zs2[i] * 0.05), "scaled frozen at index $i")
        }
    }
}
