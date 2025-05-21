package org.basaltmc.world.noise

import org.basaltmc.random.LegacyRandom
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Golden vectors captured by running the real decompiled 26.2 bytecode (`RandomSource.create`
 * backed by `LegacyRandomSource`, same pattern [org.basaltmc.random.XoroshiroRandom]'s own tests
 * use), not derived from this project's own code.
 */
class NoiseVectorsTest {
    @Test
    fun improvedNoiseMatchesVanilla() {
        val random = LegacyRandom(12345L)
        val noise = ImprovedNoise(random)
        assertEquals(92.62159543308078, noise.xo, ULP)
        assertEquals(238.8463322338665, noise.yo, ULP)
        assertEquals(213.27138533658206, noise.zo, ULP)
        assertEquals(-0.37230011176761096, noise.noise(0.0, 0.0, 0.0), ULP)
        assertEquals(-0.25374550838340176, noise.noise(10.0, 20.0, 30.0), ULP)
        assertEquals(-0.2765932621975088, noise.noise(0.5, 0.25, 0.125), ULP)
        assertEquals(-0.1923774469193794, noise.noise(-5.5, 3.3, 100.7), ULP)
    }

    @Test
    fun simplexNoiseMatchesVanilla() {
        val random = LegacyRandom(999L)
        val noise = SimplexNoise(random)
        assertEquals(0.8587266484253088, noise.getValue(0.1, 0.2), ULP)
        assertEquals(-2.3329158292673125E-16, noise.getValue(0.05, -0.05), ULP)
        assertEquals(0.30097613310751026, noise.getValue(0.1, 0.2, 0.05), ULP)
        assertEquals(0.20338001141940465, noise.getValue(-0.02, 0.03, -0.04), ULP)
    }

    @Test
    fun perlinNoiseMatchesVanilla() {
        val random = LegacyRandom(42L)
        val noise = PerlinNoise.create(random, listOf(-3, -2, -1, 0))
        assertEquals(0.03881452277675079, noise.getValue(0.0, 0.0, 0.0), ULP)
        assertEquals(0.04376609878708928, noise.getValue(100.0, 64.0, -200.0), ULP)
        assertEquals(0.10524273315881554, noise.getValue(1.25, -3.75, 8.125), ULP)
    }

    @Test
    fun normalNoiseMatchesVanilla() {
        val random = LegacyRandom(7L)
        val noise = NormalNoise.create(random, -3, doubleArrayOf(1.0, 1.0, 1.0, 1.0))
        assertEquals(-0.4715557075842656, noise.getValue(0.0, 0.0, 0.0), ULP)
        assertEquals(-0.32736478793337376, noise.getValue(12.0, 34.0, -56.0), ULP)
        assertEquals(-0.6960095970788946, noise.getValue(-1.5, 2.5, -3.5), ULP)
    }

    private companion object {
        const val ULP = 1.0E-12
    }
}
