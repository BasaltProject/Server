package org.basaltmc.random

import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Golden vectors captured by running the actual decompiled 26.2 server bytecode
 * (`net.minecraft.world.level.levelgen.WorldgenRandom`, wrapping either `XoroshiroRandomSource`
 * or `LegacyRandomSource`) through a small JVM harness, not by re-deriving the algorithm from
 * memory.
 */
class WorldgenRandomTest {
    @Test
    fun setDecorationSeedOverXoroshiroMatchesVanilla() {
        val random = WorldgenRandom(XoroshiroRandom(999L))
        val decorationSeed = random.setDecorationSeed(1234567L, 32, -16)
        assertEquals(-8234665531566012489L, decorationSeed)
        assertEquals(listOf(9, 12, 2, 0, 13, 3, 8, 3), List(8) { random.nextInt(16) })
        assertEquals(0.49740642f, random.nextFloat())
    }

    @Test
    fun setLargeFeatureWithSaltOverLegacyMatchesVanilla() {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureWithSalt(9876543L, 3, -2, 10387312)
        assertEquals(listOf(0, 11, 12, 5, 4, 9), List(6) { random.nextInt(26) })
    }

    @Test
    fun setDecorationSeedAtChunkOriginMatchesVanilla() {
        val random = WorldgenRandom(XoroshiroRandom(1L))
        random.setDecorationSeed(-42L, 0, 0)
        assertEquals(listOf(46, 7, 93, 55), List(4) { random.nextInt(96) })
    }
}
