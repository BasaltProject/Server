package org.basaltmc.world.climate

import org.basaltmc.registry.Biomes
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Golden vectors generated locally by running the real, decompiled `OverworldBiomeBuilder` from
 * the 26.2 server jar (never committed to this repository): a small standalone harness dumped every
 * `(ParameterPoint, biome)` pair `addBiomes` produces, and diffed it against this same class's own
 * output built the same way. The two were byte-for-byte identical across all 7594 regions; this
 * test keeps a representative sample of that comparison as a lasting regression check. Only numeric
 * threshold data and public vanilla biome path names are stored here, no jar content.
 */
class OverworldBiomeBuilderTest {
    private fun point(
        temperature: Pair<Long, Long>,
        humidity: Pair<Long, Long>,
        continentalness: Pair<Long, Long>,
        erosion: Pair<Long, Long>,
        depth: Pair<Long, Long>,
        weirdness: Pair<Long, Long>,
        offset: Long,
    ): ParameterPoint =
        ParameterPoint(
            Parameter(temperature.first, temperature.second),
            Parameter(humidity.first, humidity.second),
            Parameter(continentalness.first, continentalness.second),
            Parameter(erosion.first, erosion.second),
            Parameter(depth.first, depth.second),
            Parameter(weirdness.first, weirdness.second),
            offset,
        )

    @Test
    fun matchesTheRealJarAcrossASampleOfEveryClimateRegion() {
        val golden =
            listOf(
                point(-10000L to 10000L, -10000L to 10000L, -12000L to -10500L, -10000L to 10000L, 0L to 0L, -10000L to 10000L, 0L) to Biomes.MUSHROOM_FIELDS,
                point(-4500L to -1500L, 1000L to 3000L, -1100L to 10000L, -10000L to -7799L, 10000L to 10000L, -10000L to -9333L, 0L) to Biomes.GROVE,
                point(2000L to 5500L, -1000L to 1000L, -1900L to 300L, -2225L to 500L, 0L to 0L, -10000L to -9333L, 0L) to Biomes.FOREST,
                point(-10000L to -4500L, -3500L to -1000L, -1900L to 10000L, 5500L to 10000L, 10000L to 10000L, -9333L to -7666L, 0L) to Biomes.SNOWY_PLAINS,
                point(-1500L to 2000L, -1000L to 1000L, -1100L to 300L, -10000L to -7799L, 0L to 0L, -9333L to -7666L, 0L) to Biomes.GROVE,
                point(5500L to 10000L, -1000L to 1000L, 300L to 10000L, -10000L to -7799L, 10000L to 10000L, -9333L to -7666L, 0L) to Biomes.BADLANDS,
                point(-4500L to -1500L, 1000L to 3000L, -1900L to 10000L, 500L to 4500L, 0L to 0L, -7666L to -5666L, 0L) to Biomes.TAIGA,
                point(5500L to 10000L, -10000L to -3500L, 3000L to 10000L, -2225L to 500L, 10000L to 10000L, -7666L to -5666L, 0L) to Biomes.BADLANDS,
                point(-4500L to -1500L, -3500L to -1000L, -1900L to 300L, -3750L to 500L, 0L to 0L, -5666L to -4000L, 0L) to Biomes.PLAINS,
                point(2000L to 5500L, -3500L to -1000L, 300L to 10000L, -3750L to -2225L, 10000L to 10000L, -5666L to -4000L, 0L) to Biomes.SAVANNA_PLATEAU,
                point(-10000L to -4500L, -3500L to -1000L, -1100L to 300L, -3750L to -2225L, 0L to 0L, -4000L to -2666L, 0L) to Biomes.SNOWY_PLAINS,
                point(-1500L to 2000L, -10000L to -3500L, 300L to 3000L, -3750L to -2225L, 10000L to 10000L, -4000L to -2666L, 0L) to Biomes.FLOWER_FOREST,
                point(2000L to 5500L, 3000L to 10000L, -1900L to -1100L, 4500L to 5500L, 0L to 0L, -4000L to -2666L, 0L) to Biomes.BEACH,
                point(-10000L to -4500L, 3000L to 10000L, -1100L to 10000L, 5500L to 10000L, 10000L to 10000L, -2666L to -500L, 0L) to Biomes.TAIGA,
                point(2000L to 5500L, 1000L to 3000L, 300L to 10000L, -10000L to -3750L, 0L to 0L, -2666L to -500L, 0L) to Biomes.JUNGLE,
                point(-10000L to -4500L, -1000L to 1000L, 300L to 10000L, -10000L to -3750L, 10000L to 10000L, 500L to 2666L, 0L) to Biomes.GROVE,
                point(2000L to 5500L, -10000L to -3500L, -1100L to 300L, -10000L to -3750L, 0L to 0L, 500L to 2666L, 0L) to Biomes.SAVANNA,
                point(-10000L to -4500L, -1000L to 1000L, -1100L to 10000L, -10000L to -7799L, 10000L to 10000L, 2666L to 4000L, 0L) to Biomes.GROVE,
                point(-1500L to 2000L, -3500L to -1000L, -1900L to -1100L, 5500L to 10000L, 0L to 0L, 2666L to 4000L, 0L) to Biomes.PLAINS,
                point(5500L to 10000L, -1000L to 1000L, -1100L to 10000L, -10000L to -7799L, 10000L to 10000L, 2666L to 4000L, 0L) to Biomes.BADLANDS,
                point(-4500L to -1500L, -1000L to 1000L, 300L to 10000L, -10000L to -7799L, 0L to 0L, 4000L to 5666L, 0L) to Biomes.FROZEN_PEAKS,
                point(2000L to 5500L, -1000L to 1000L, -1100L to 300L, -7799L to -3750L, 10000L to 10000L, 4000L to 5666L, 0L) to Biomes.PLAINS,
                point(-10000L to -4500L, -1000L to 1000L, 300L to 10000L, 4500L to 5500L, 0L to 0L, 5666L to 7666L, 0L) to Biomes.WINDSWEPT_HILLS,
                point(-1500L to 2000L, 3000L to 10000L, -1900L to 300L, 4500L to 5500L, 10000L to 10000L, 5666L to 7666L, 0L) to Biomes.WINDSWEPT_FOREST,
                point(-10000L to -4500L, -3500L to -1000L, 300L to 10000L, -3750L to -2225L, 0L to 0L, 7666L to 9333L, 0L) to Biomes.SNOWY_PLAINS,
                point(-1500L to 2000L, -3500L to -1000L, 300L to 3000L, -2225L to 500L, 10000L to 10000L, 7666L to 9333L, 0L) to Biomes.PLAINS,
                point(5500L to 10000L, -3500L to -1000L, -1900L to 10000L, 500L to 4500L, 0L to 0L, 7666L to 9333L, 0L) to Biomes.DESERT,
                point(-4500L to -1500L, -3500L to -1000L, 3000L to 10000L, -7799L to -3750L, 10000L to 10000L, 9333L to 10000L, 0L) to Biomes.MEADOW,
                point(2000L to 5500L, -3500L to -1000L, 300L to 3000L, -3750L to -2225L, 0L to 0L, 9333L to 10000L, 0L) to Biomes.SAVANNA,
            )

        val built = OverworldBiomeBuilder().build().values
        assertEquals(7594, built.size)
        for ((expectedPoint, expectedBiome) in golden) {
            assertTrue(
                built.any { (point, biome) -> point == expectedPoint && biome == expectedBiome },
                "expected $expectedPoint -> ${expectedBiome.id} to be present",
            )
        }
    }
}
