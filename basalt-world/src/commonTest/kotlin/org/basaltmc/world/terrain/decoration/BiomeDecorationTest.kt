package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class BiomeDecorationTest {
    @Test
    fun resolvesAllFiftyFiveOverworldBiomes() {
        assertEquals(55, overworldBiomePaths.size)
        for (path in overworldBiomePaths) {
            assertNotNull(BiomeDecoration.forBiome(Identifier.vanilla(path)), "missing decoration for minecraft:$path")
        }
    }

    private val overworldBiomePaths =
        setOf(
            "badlands",
            "bamboo_jungle",
            "beach",
            "birch_forest",
            "cherry_grove",
            "cold_ocean",
            "dark_forest",
            "deep_cold_ocean",
            "deep_dark",
            "deep_frozen_ocean",
            "deep_lukewarm_ocean",
            "deep_ocean",
            "desert",
            "dripstone_caves",
            "eroded_badlands",
            "flower_forest",
            "forest",
            "frozen_ocean",
            "frozen_peaks",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jagged_peaks",
            "jungle",
            "lukewarm_ocean",
            "lush_caves",
            "mangrove_swamp",
            "meadow",
            "mushroom_fields",
            "ocean",
            "old_growth_birch_forest",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "pale_garden",
            "plains",
            "river",
            "savanna",
            "savanna_plateau",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
            "sparse_jungle",
            "stony_peaks",
            "stony_shore",
            "sulfur_caves",
            "sunflower_plains",
            "swamp",
            "taiga",
            "warm_ocean",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
            "windswept_savanna",
            "wooded_badlands",
        )
}
