package org.basaltmc.server.runtime.world

internal enum class PandaGene(
    internal val serializedName: String,
    internal val id: Int,
    internal val recessive: Boolean = false,
) {
    NORMAL("normal", 0),
    LAZY("lazy", 1),
    WORRIED("worried", 2),
    PLAYFUL("playful", 3),
    BROWN("brown", 4, true),
    WEAK("weak", 5, true),
    AGGRESSIVE("aggressive", 6),
}

/** Immutable lookup tables used by natural mob spawn finalizers. */
internal object MobSpawnVariantData {
    internal val PANDA_GENES_BY_NAME: Map<String, PandaGene> =
        PandaGene.entries.associateBy(PandaGene::serializedName)

    internal val CHICKEN_SOUND_VARIANTS: List<String> = listOf("classic", "picky")
    internal val COW_SOUND_VARIANTS: List<String> = listOf("classic", "moody")
    internal val PIG_SOUND_VARIANTS: List<String> = listOf("classic", "big", "mini")
    internal val WOLF_VARIANTS: List<String> =
        listOf("pale", "spotted", "snowy", "black", "ashen", "rusty", "woods", "chestnut", "striped")
    internal val WOLF_SOUND_VARIANTS: List<String> =
        listOf("classic", "puglin", "sad", "angry", "grumpy", "big", "cute")

    internal val TROPICAL_COMMON_VARIANTS: List<Int> =
        listOf(
            packTropicalFish(7, 1, 7),
            packTropicalFish(6, 7, 7),
            packTropicalFish(6, 7, 11),
            packTropicalFish(11, 0, 7),
            packTropicalFish(1, 11, 7),
            packTropicalFish(0, 1, 0),
            packTropicalFish(5, 6, 3),
            packTropicalFish(9, 10, 4),
            packTropicalFish(11, 0, 14),
            packTropicalFish(5, 0, 4),
            packTropicalFish(8, 0, 7),
            packTropicalFish(11, 0, 1),
            packTropicalFish(3, 9, 6),
            packTropicalFish(4, 5, 3),
            packTropicalFish(10, 14, 0),
            packTropicalFish(2, 7, 14),
            packTropicalFish(9, 14, 0),
            packTropicalFish(6, 0, 4),
            packTropicalFish(0, 14, 0),
            packTropicalFish(1, 7, 0),
            packTropicalFish(3, 9, 4),
            packTropicalFish(6, 4, 4),
        )

    internal val WHITE_RABBIT_BIOMES: Set<String> =
        setOf(
            "frozen_ocean",
            "frozen_peaks",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jagged_peaks",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
        )

    internal val WARM_FARM_BIOMES: Set<String> =
        setOf(
            "badlands",
            "bamboo_jungle",
            "basalt_deltas",
            "crimson_forest",
            "deep_lukewarm_ocean",
            "desert",
            "eroded_badlands",
            "jungle",
            "lukewarm_ocean",
            "mangrove_swamp",
            "nether_wastes",
            "savanna",
            "savanna_plateau",
            "soul_sand_valley",
            "sparse_jungle",
            "warm_ocean",
            "warped_forest",
            "windswept_savanna",
            "wooded_badlands",
        )

    internal val COLD_FARM_BIOMES: Set<String> =
        setOf(
            "cold_ocean",
            "deep_cold_ocean",
            "deep_dark",
            "deep_frozen_ocean",
            "end_barrens",
            "end_highlands",
            "end_midlands",
            "frozen_ocean",
            "frozen_peaks",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jagged_peaks",
            "old_growth_pine_taiga",
            "old_growth_spruce_taiga",
            "small_end_islands",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
            "stony_peaks",
            "taiga",
            "the_end",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
        )

    internal val WARM_FROG_BIOMES: Set<String> =
        setOf(
            "badlands",
            "bamboo_jungle",
            "basalt_deltas",
            "crimson_forest",
            "desert",
            "eroded_badlands",
            "jungle",
            "mangrove_swamp",
            "nether_wastes",
            "savanna",
            "savanna_plateau",
            "soul_sand_valley",
            "sparse_jungle",
            "warm_ocean",
            "warped_forest",
            "windswept_savanna",
            "wooded_badlands",
        )

    internal val COLD_FROG_BIOMES: Set<String> =
        setOf(
            "deep_dark",
            "deep_frozen_ocean",
            "end_barrens",
            "end_highlands",
            "end_midlands",
            "frozen_ocean",
            "frozen_peaks",
            "frozen_river",
            "grove",
            "ice_spikes",
            "jagged_peaks",
            "small_end_islands",
            "snowy_beach",
            "snowy_plains",
            "snowy_slopes",
            "snowy_taiga",
            "the_end",
        )

    internal val SAVANNA_BIOMES: Set<String> = setOf("savanna", "savanna_plateau", "windswept_savanna")
    internal val JUNGLE_BIOMES: Set<String> = setOf("bamboo_jungle", "jungle", "sparse_jungle")
    internal val BADLANDS_BIOMES: Set<String> = setOf("badlands", "eroded_badlands", "wooded_badlands")

    private fun packTropicalFish(
        pattern: Int,
        baseColor: Int,
        patternColor: Int,
    ): Int {
        val large = if (pattern >= TROPICAL_SMALL_PATTERNS) 1 else 0
        return large or ((pattern % TROPICAL_SMALL_PATTERNS) shl 8) or (baseColor shl 16) or (patternColor shl 24)
    }

    private const val TROPICAL_SMALL_PATTERNS = 6
}
