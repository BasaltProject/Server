package org.basaltmc.registry

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class BiomesTest {
    @Test
    fun plainsIsRegisteredAtItsBootstrapOrderId() {
        assertEquals(Identifier.vanilla("plains"), Biomes.PLAINS.id)
        assertEquals(Biomes.PLAINS, Biomes.REGISTRY.byId(0))
    }

    @Test
    fun everyOverworldBiomeFromTheRealBootstrapOrderIsRegistered() {
        val expected =
            listOf(
                "plains",
                "sunflower_plains",
                "snowy_plains",
                "ice_spikes",
                "desert",
                "swamp",
                "mangrove_swamp",
                "forest",
                "flower_forest",
                "birch_forest",
                "dark_forest",
                "pale_garden",
                "old_growth_birch_forest",
                "old_growth_pine_taiga",
                "old_growth_spruce_taiga",
                "taiga",
                "snowy_taiga",
                "savanna",
                "savanna_plateau",
                "windswept_hills",
                "windswept_gravelly_hills",
                "windswept_forest",
                "windswept_savanna",
                "jungle",
                "sparse_jungle",
                "bamboo_jungle",
                "badlands",
                "eroded_badlands",
                "wooded_badlands",
                "meadow",
                "cherry_grove",
                "grove",
                "snowy_slopes",
                "frozen_peaks",
                "jagged_peaks",
                "stony_peaks",
                "river",
                "frozen_river",
                "beach",
                "snowy_beach",
                "stony_shore",
                "warm_ocean",
                "lukewarm_ocean",
                "deep_lukewarm_ocean",
                "ocean",
                "deep_ocean",
                "cold_ocean",
                "deep_cold_ocean",
                "frozen_ocean",
                "deep_frozen_ocean",
                "mushroom_fields",
                "dripstone_caves",
                "lush_caves",
                "deep_dark",
                "sulfur_caves",
            )
        expected.forEachIndexed { id, path ->
            val biome = Biomes.REGISTRY.byId(id)
            assertNotNull(biome, "expected a biome registered at id $id")
            assertEquals(Identifier.vanilla(path), biome.id)
        }
    }
}
