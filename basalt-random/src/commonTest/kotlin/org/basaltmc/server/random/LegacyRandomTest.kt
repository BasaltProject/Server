package org.basaltmc.server.random

import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Golden vectors captured by running the actual decompiled 26.2 server bytecode
 * (`net.minecraft.world.level.levelgen.LegacyRandomSource`) through a small JVM harness, not by
 * re-deriving the algorithm from memory.
 */
class LegacyRandomTest {
    private fun ints(
        seed: Long,
        count: Int,
    ): List<Int> {
        val random = LegacyRandom(seed)
        return List(count) { random.nextInt() }
    }

    @Test
    fun nextIntMatchesVanilla() {
        assertEquals(
            listOf(-1155484576, -723955400, 1033096058, -1690734402, -1557280266),
            ints(0L, 5),
        )
        assertEquals(
            listOf(-1155869325, 431529176, 1761283695, 1749940626, 892128508),
            ints(1L, 5),
        )
        assertEquals(
            listOf(-1170105035, 234785527, -1360544799, 205897768, 1325939940),
            ints(42L, 5),
        )
        assertEquals(
            listOf(1155099827, 1887904451, 52699159, -1941176418, -1451336087),
            ints(-1L, 5),
        )
        assertEquals(
            listOf(-1442945365, -1016548095, 1962592967, 1094656688, 1677212580),
            ints(123456789L, 5),
        )
    }

    private fun boundedInts(
        seed: Long,
        bound: Int,
        count: Int,
    ): List<Int> {
        val random = LegacyRandom(seed)
        return List(count) { random.nextInt(bound) }
    }

    @Test
    fun nextIntBoundMatchesVanilla() {
        assertEquals(listOf(0, 0, 0, 0, 0), boundedInts(0L, 1, 5))
        assertEquals(listOf(11, 13, 3, 9, 10), boundedInts(0L, 16, 5))
        assertEquals(listOf(60, 48, 29, 47, 15), boundedInts(0L, 100, 5))
        assertEquals(listOf(5, 2, 4, 2, 4), boundedInts(0L, 7, 5))
        assertEquals(listOf(0, 0, 0, 0, 0), boundedInts(42L, 1, 5))
        assertEquals(listOf(11, 0, 10, 0, 4), boundedInts(42L, 16, 5))
        assertEquals(listOf(30, 63, 48, 84, 70), boundedInts(42L, 100, 5))
        assertEquals(listOf(1, 5, 6, 3, 5), boundedInts(42L, 7, 5))
    }

    @Test
    fun fullSequenceMatchesVanillaForSeedZero() {
        val random = LegacyRandom(0L)
        assertEquals(-4962768465676381896L, random.nextLong())
        assertEquals(4437113781045784766L, random.nextLong())
        assertEquals(-6688467811848818630L, random.nextLong())
        assertEquals(0.550437f, random.nextFloat())
        assertEquals(0.1170066f, random.nextFloat())
        assertEquals(0.59754527f, random.nextFloat())
        assertEquals(0.7815346320453048, random.nextDouble())
        assertEquals(0.2527761665759859, random.nextDouble())
        assertEquals(0.6130357680446138, random.nextDouble())
        assertEquals(true, random.nextBoolean())
        assertEquals(true, random.nextBoolean())
        assertEquals(false, random.nextBoolean())
        assertEquals(0.17268833153399873, random.nextGaussian())
        assertEquals(-0.08807455272616774, random.nextGaussian())
        assertEquals(-0.4118293663901143, random.nextGaussian())
    }

    @Test
    fun fullSequenceMatchesVanillaForSeed42() {
        val random = LegacyRandom(42L)
        assertEquals(-5025562857975149833L, random.nextLong())
        assertEquals(-5843495416241995736L, random.nextLong())
        assertEquals(5694868678511409995L, random.nextLong())
        assertEquals(0.27707845f, random.nextFloat())
        assertEquals(0.70771056f, random.nextFloat())
        assertEquals(0.6655489f, random.nextFloat())
        assertEquals(0.09132461105841794, random.nextDouble())
        assertEquals(0.4512572194454507, random.nextDouble())
        assertEquals(0.3816430907807956, random.nextDouble())
        assertEquals(true, random.nextBoolean())
        assertEquals(false, random.nextBoolean())
        assertEquals(true, random.nextBoolean())
        assertEquals(-0.40086993610549193, random.nextGaussian())
        assertEquals(1.577411619884327, random.nextGaussian())
        assertEquals(-0.4088580666890422, random.nextGaussian())
    }

    @Test
    fun forkPositionalMatchesVanilla() {
        val factory = LegacyRandom(42L).forkPositional()
        val cases =
            listOf(
                Triple(0, 0, 0) to (2040010338 to -1911889734),
                Triple(1, 2, 3) to (-1062610619 to 511209597),
                Triple(-5, 64, 100) to (-1147523588 to -461951395),
                Triple(123, -45, 678) to (1038672630 to 1184260035),
                Triple(1000000, 0, -1000000) to (2082118675 to -270010132),
            )
        for ((pos, expected) in cases) {
            val random = factory.at(pos.first, pos.second, pos.third)
            assertEquals(expected.first, random.nextInt())
            assertEquals(expected.second, random.nextInt())
        }
    }
}
