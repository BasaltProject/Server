package org.basaltmc.server.random

import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * Golden vectors captured by running the actual decompiled 26.2 server bytecode
 * (`net.minecraft.world.level.levelgen.XoroshiroRandomSource`) through a small JVM harness, not
 * by re-deriving the algorithm from memory.
 */
class XoroshiroRandomTest {
    private fun ints(
        seed: Long,
        count: Int,
    ): List<Int> {
        val random = XoroshiroRandom(seed)
        return List(count) { random.nextInt() }
    }

    private fun longs(
        seed: Long,
        count: Int,
    ): List<Long> {
        val random = XoroshiroRandom(seed)
        return List(count) { random.nextLong() }
    }

    @Test
    fun nextIntMatchesVanilla() {
        assertEquals(listOf(-160476802, 781697906, 653572596, 1337520923, -505875771), ints(0L, 5))
        assertEquals(listOf(1734564350, 836234122, 825264738, -1425890201, 767430484), ints(1L, 5))
        assertEquals(listOf(1774573855, 1357444267, -610653507, 2070806918, -1441753141), ints(42L, 5))
        assertEquals(listOf(-1451817517, 2009620260, 916420186, -51837615, 933551611), ints(-1L, 5))
        assertEquals(listOf(1822098705, 1667009048, 231980577, -1919295214, -20373141), ints(123456789L, 5))
    }

    @Test
    fun nextLongMatchesVanilla() {
        assertEquals(
            listOf(
                3038984756725240190L,
                -3694039286755638414L,
                4633751808701151732L,
                2160572957309072155L,
                1839370574944072389L,
            ),
            longs(0L, 5),
        )
        assertEquals(
            listOf(
                -1033667707219518978L,
                6451672561743293322L,
                -1821890263888393630L,
                890086654470169703L,
                8094835630745194324L,
            ),
            longs(1L, 5),
        )
        assertEquals(
            listOf(
                -4695948378737616609L,
                7341713790291473579L,
                -7542733514721318211L,
                4888889476139319686L,
                8419651034331256779L,
            ),
            longs(42L, 5),
        )
        assertEquals(
            listOf(
                -8676505878415342125L,
                -868585888688873692L,
                -6331679347063163302L,
                -2068491455652362927L,
                -5626054917968568837L,
            ),
            longs(-1L, 5),
        )
        assertEquals(
            listOf(
                3219654894476264721L,
                8777179534091651608L,
                6434035011650043425L,
                -8486945822236614382L,
                3449493904201163115L,
            ),
            longs(123456789L, 5),
        )
    }

    private fun boundedInts(
        seed: Long,
        bound: Int,
        count: Int,
    ): List<Int> {
        val random = XoroshiroRandom(seed)
        return List(count) { random.nextInt(bound) }
    }

    @Test
    fun nextIntBoundMatchesVanilla() {
        assertEquals(listOf(0, 0, 0, 0, 0), boundedInts(0L, 1, 5))
        assertEquals(listOf(15, 2, 2, 4, 14), boundedInts(0L, 16, 5))
        assertEquals(listOf(96, 18, 15, 31, 88), boundedInts(0L, 100, 5))
        assertEquals(listOf(6, 1, 1, 2, 6), boundedInts(0L, 7, 5))
        assertEquals(listOf(0, 0, 0, 0, 0), boundedInts(42L, 1, 5))
        assertEquals(listOf(6, 5, 13, 7, 10), boundedInts(42L, 16, 5))
        assertEquals(listOf(41, 31, 85, 48, 66), boundedInts(42L, 100, 5))
        assertEquals(listOf(2, 2, 6, 3, 4), boundedInts(42L, 7, 5))
    }

    @Test
    fun fullSequenceMatchesVanillaForSeedZero() {
        val random = XoroshiroRandom(0L)
        assertEquals(0.16474366f, random.nextFloat())
        assertEquals(0.7997457f, random.nextFloat())
        assertEquals(0.25119615f, random.nextFloat())
        assertEquals(0.11712489470639631, random.nextDouble())
        assertEquals(0.0997124786680137, random.nextDouble())
        assertEquals(0.7566797430601416, random.nextDouble())
        assertEquals(false, random.nextBoolean())
        assertEquals(true, random.nextBoolean())
        assertEquals(true, random.nextBoolean())
        assertEquals(0.31104841012084117, random.nextGaussian())
        assertEquals(-1.0186152786011977, random.nextGaussian())
        assertEquals(-0.03511119974585956, random.nextGaussian())
    }

    @Test
    fun fullSequenceMatchesVanillaForSeed42() {
        val random = XoroshiroRandom(42L)
        assertEquals(0.7454321f, random.nextFloat())
        assertEquals(0.39799505f, random.nextFloat())
        assertEquals(0.59110755f, random.nextFloat())
        assertEquals(0.2650272295535885, random.nextDouble())
        assertEquals(0.45643019714958855, random.nextDouble())
        assertEquals(0.6480715228911569, random.nextDouble())
        assertEquals(true, random.nextBoolean())
        assertEquals(false, random.nextBoolean())
        assertEquals(true, random.nextBoolean())
        assertEquals(3.1556335355420093, random.nextGaussian())
        assertEquals(-0.07157280316880266, random.nextGaussian())
        assertEquals(-0.017170294951799683, random.nextGaussian())
    }

    @Test
    fun forkPositionalMatchesVanilla() {
        val factory = XoroshiroRandom(42L).forkPositional()
        val cases =
            listOf(
                Triple(0, 0, 0) to (89793166 to -1194968252),
                Triple(1, 2, 3) to (1942892291 to -1485131184),
                Triple(-5, 64, 100) to (-1435539674 to 262546357),
                Triple(123, -45, 678) to (1740712342 to -539362039),
                Triple(1000000, 0, -1000000) to (-600618616 to 1099295309),
            )
        for ((pos, expected) in cases) {
            val random = factory.at(pos.first, pos.second, pos.third)
            assertEquals(expected.first, random.nextInt())
            assertEquals(expected.second, random.nextInt())
        }
    }
}
