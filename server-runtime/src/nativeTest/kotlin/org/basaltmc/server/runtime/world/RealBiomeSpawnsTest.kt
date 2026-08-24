package org.basaltmc.server.runtime.world

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** parity: `MobSpawnSettings`, the `spawners` block of a biome file. */
class RealBiomeSpawnsTest {
    @Test
    fun withoutTheJarNothingSpawns() {
        val spawns = RealBiomeSpawns(serverJar = null)

        assertTrue(spawns.listFor(Identifier.vanilla("plains"), "creature").isEmpty())
        assertTrue(spawns.of(Identifier.vanilla("plains")).isEmpty())
    }

    /**
     * The parser itself, against the shape a real biome file has. The file is the operator's, so the
     * fixture here is written by hand rather than copied out of the jar.
     */
    @Test
    fun itReadsTheSpawnListsOfABiome() {
        val parsed = ParsedForTest.parse(SAMPLE)

        val creatures = parsed["creature"].orEmpty()
        assertEquals(2, creatures.size)
        assertEquals(Identifier.vanilla("sheep"), creatures[0].type)
        assertEquals(12, creatures[0].weight)
        assertEquals(4, creatures[0].minCount)
        assertEquals(4, creatures[0].maxCount)
        assertEquals(Identifier.vanilla("horse"), creatures[1].type)
        assertEquals(2, creatures[1].minCount)
        assertEquals(6, creatures[1].maxCount)

        assertEquals(1, parsed["monster"]?.size)
        assertTrue(parsed["axolotls"].orEmpty().isEmpty(), "an empty list is still a list")
        assertEquals(0.1f, RealBiomeSpawns.creatureProbabilityForTest(SAMPLE))
        assertEquals(
            0.03f,
            RealBiomeSpawns.creatureProbabilityForTest("""{"creature_spawn_probability":0.03,"spawners":{}}"""),
        )
    }

    private companion object {
        /** The shape of a biome's `spawners`, with the surrounding fields a real file also has. */
        val SAMPLE =
            """
            {
              "downfall": 0.4,
              "spawners": {
                "axolotls": [],
                "creature": [
                  { "type": "minecraft:sheep", "maxCount": 4, "minCount": 4, "weight": 12 },
                  { "type": "minecraft:horse", "maxCount": 6, "minCount": 2, "weight": 5 }
                ],
                "monster": [
                  { "type": "minecraft:spider", "maxCount": 4, "minCount": 4, "weight": 100 }
                ]
              },
              "temperature": 0.8
            }
            """.trimIndent()
    }
}

/** Reaches the parser without a jar: it is private to the loader, which takes one. */
private object ParsedForTest {
    fun parse(json: String): Map<String, List<SpawnerEntry>> = RealBiomeSpawns.parseForTest(json)
}
