package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom

internal data class Target(
    val replaceable: Set<String>,
    val stateId: Int,
)

internal data class ConfiguredOre(
    val size: Int,
    val discardChanceOnAirExposure: Float,
    val targets: List<Target>,
)

internal data class PlacedOre(
    val globalIndex: Int,
    val count: Count,
    val height: Height,
    val biomes: Set<String>,
    val feature: ConfiguredOre,
)

internal fun interface Count {
    fun sample(random: BasaltRandom): Int
}

internal fun interface Height {
    fun sample(random: BasaltRandom): Int
}

private fun fixedCount(value: Int): Count = Count { value }

private fun uniformCount(
    min: Int,
    max: Int,
): Count = Count { random -> min + random.nextInt(max - min + 1) }

private fun uniformHeight(
    min: Int,
    max: Int,
): Height = Height { random -> min + random.nextInt(max - min + 1) }

private fun triangleHeight(
    min: Int,
    max: Int,
): Height =
    Height { random ->
        val range = max - min
        val firstHalf = range / 2
        val secondHalf = range - firstHalf
        min + random.nextInt(secondHalf + 1) + random.nextInt(firstHalf + 1)
    }

private fun rarityCount(chance: Int): Count = Count { random -> if (random.nextFloat() < 1.0f / chance) 1 else 0 }

private fun singleTarget(
    size: Int,
    discard: Float,
    block: String,
    replaceable: Set<String> = BASE_STONE_OVERWORLD,
): ConfiguredOre = ConfiguredOre(size, discard, listOf(Target(replaceable, stateId(block))))

private fun pairedTarget(
    size: Int,
    discard: Float,
    stoneBlock: String,
    deepslateBlock: String,
): ConfiguredOre =
    ConfiguredOre(
        size,
        discard,
        listOf(
            Target(STONE_ORE_REPLACEABLES, stateId(stoneBlock)),
            Target(DEEPSLATE_ORE_REPLACEABLES, stateId(deepslateBlock)),
        ),
    )

private fun placed(
    index: Int,
    count: Count,
    height: Height,
    feature: ConfiguredOre,
    biomes: Set<String> = ALL_BIOMES,
): PlacedOre = PlacedOre(index, count, height, biomes, feature)

internal val ALL_BIOMES: Set<String> =
    setOf(
        "plains",
        "sunflower_plains",
        "desert",
        "windswept_hills",
        "windswept_gravelly_hills",
        "windswept_forest",
        "windswept_savanna",
        "forest",
        "flower_forest",
        "birch_forest",
        "old_growth_birch_forest",
        "dark_forest",
        "old_growth_pine_taiga",
        "old_growth_spruce_taiga",
        "taiga",
        "snowy_taiga",
        "savanna",
        "savanna_plateau",
        "badlands",
        "wooded_badlands",
        "eroded_badlands",
        "jungle",
        "sparse_jungle",
        "bamboo_jungle",
        "swamp",
        "mangrove_swamp",
        "river",
        "frozen_river",
        "beach",
        "snowy_beach",
        "stony_shore",
        "snowy_plains",
        "ice_spikes",
        "ocean",
        "deep_ocean",
        "warm_ocean",
        "lukewarm_ocean",
        "deep_lukewarm_ocean",
        "cold_ocean",
        "deep_cold_ocean",
        "frozen_ocean",
        "deep_frozen_ocean",
        "mushroom_fields",
        "cherry_grove",
        "pale_garden",
        "meadow",
        "grove",
        "snowy_slopes",
        "frozen_peaks",
        "jagged_peaks",
        "stony_peaks",
        "dripstone_caves",
        "lush_caves",
        "deep_dark",
        "sulfur_caves",
    )
private val MOUNTAIN_BIOMES: Set<String> =
    setOf(
        "windswept_hills",
        "windswept_gravelly_hills",
        "windswept_forest",
        "cherry_grove",
        "meadow",
        "grove",
        "snowy_slopes",
        "frozen_peaks",
        "jagged_peaks",
        "stony_peaks",
    )
private val BADLANDS_BIOMES: Set<String> = setOf("badlands", "wooded_badlands", "eroded_badlands")
internal val DEFAULT_DISK_BIOMES: Set<String> = ALL_BIOMES - setOf("swamp", "mangrove_swamp")
private val BASE_STONE_OVERWORLD: Set<String> = setOf("stone", "granite", "diorite", "andesite", "tuff", "deepslate")
private val STONE_ORE_REPLACEABLES: Set<String> = setOf("stone", "granite", "diorite", "andesite")
private val DEEPSLATE_ORE_REPLACEABLES: Set<String> = setOf("deepslate", "tuff")

// parity: FeatureSorter global indexes for the 55 Overworld biome feature lists in 26.2.
private val FEATURES: List<PlacedOre> =
    listOf(
        placed(0, fixedCount(7), uniformHeight(0, 160), singleTarget(33, 0.0f, "dirt")),
        placed(1, fixedCount(14), uniformHeight(-64, 319), singleTarget(33, 0.0f, "gravel")),
        placed(2, rarityCount(6), uniformHeight(64, 128), singleTarget(64, 0.0f, "granite")),
        placed(3, fixedCount(2), uniformHeight(0, 60), singleTarget(64, 0.0f, "granite")),
        placed(4, rarityCount(6), uniformHeight(64, 128), singleTarget(64, 0.0f, "diorite")),
        placed(5, fixedCount(2), uniformHeight(0, 60), singleTarget(64, 0.0f, "diorite")),
        placed(6, rarityCount(6), uniformHeight(64, 128), singleTarget(64, 0.0f, "andesite")),
        placed(7, fixedCount(2), uniformHeight(0, 60), singleTarget(64, 0.0f, "andesite")),
        placed(8, fixedCount(2), uniformHeight(-64, 0), singleTarget(64, 0.0f, "tuff")),
        placed(
            9,
            fixedCount(30),
            uniformHeight(136, 319),
            pairedTarget(17, 0.0f, "coal_ore", "deepslate_coal_ore"),
        ),
        placed(
            10,
            fixedCount(20),
            triangleHeight(0, 192),
            pairedTarget(17, 0.5f, "coal_ore", "deepslate_coal_ore"),
        ),
        placed(
            11,
            fixedCount(90),
            triangleHeight(80, 384),
            pairedTarget(9, 0.0f, "iron_ore", "deepslate_iron_ore"),
        ),
        placed(
            12,
            fixedCount(10),
            triangleHeight(-24, 56),
            pairedTarget(9, 0.0f, "iron_ore", "deepslate_iron_ore"),
        ),
        placed(13, fixedCount(10), uniformHeight(-64, 72), pairedTarget(4, 0.0f, "iron_ore", "deepslate_iron_ore")),
        placed(14, fixedCount(4), triangleHeight(-64, 32), pairedTarget(9, 0.5f, "gold_ore", "deepslate_gold_ore")),
        placed(
            15,
            uniformCount(0, 1),
            uniformHeight(-64, -48),
            pairedTarget(9, 0.5f, "gold_ore", "deepslate_gold_ore"),
        ),
        placed(
            16,
            fixedCount(4),
            uniformHeight(-64, 15),
            pairedTarget(8, 0.0f, "redstone_ore", "deepslate_redstone_ore"),
        ),
        placed(
            17,
            fixedCount(8),
            triangleHeight(-96, -32),
            pairedTarget(8, 0.0f, "redstone_ore", "deepslate_redstone_ore"),
        ),
        placed(
            18,
            fixedCount(7),
            triangleHeight(-144, 16),
            pairedTarget(4, 0.5f, "diamond_ore", "deepslate_diamond_ore"),
        ),
        placed(
            19,
            fixedCount(2),
            uniformHeight(-64, -4),
            pairedTarget(8, 0.5f, "diamond_ore", "deepslate_diamond_ore"),
        ),
        placed(
            20,
            rarityCount(9),
            triangleHeight(-144, 16),
            pairedTarget(12, 0.7f, "diamond_ore", "deepslate_diamond_ore"),
        ),
        placed(
            21,
            fixedCount(4),
            triangleHeight(-144, 16),
            pairedTarget(8, 1.0f, "diamond_ore", "deepslate_diamond_ore"),
        ),
        placed(
            22,
            fixedCount(2),
            triangleHeight(-32, 32),
            pairedTarget(7, 0.0f, "lapis_ore", "deepslate_lapis_ore"),
        ),
        placed(
            23,
            fixedCount(4),
            uniformHeight(-64, 64),
            pairedTarget(7, 1.0f, "lapis_ore", "deepslate_lapis_ore"),
        ),
        placed(
            24,
            fixedCount(16),
            triangleHeight(-16, 112),
            pairedTarget(20, 0.0f, "copper_ore", "deepslate_copper_ore"),
            setOf("dripstone_caves"),
        ),
        placed(
            25,
            fixedCount(16),
            triangleHeight(-16, 112),
            pairedTarget(10, 0.0f, "copper_ore", "deepslate_copper_ore"),
            ALL_BIOMES - "dripstone_caves",
        ),
        placed(27, fixedCount(46), uniformHeight(-64, 256), singleTarget(33, 0.0f, "clay"), setOf("lush_caves")),
        placed(
            28,
            fixedCount(50),
            uniformHeight(32, 256),
            pairedTarget(9, 0.0f, "gold_ore", "deepslate_gold_ore"),
            BADLANDS_BIOMES,
        ),
        placed(
            33,
            fixedCount(100),
            triangleHeight(-16, 480),
            pairedTarget(3, 0.0f, "emerald_ore", "deepslate_emerald_ore"),
            MOUNTAIN_BIOMES,
        ),
    )
internal val FEATURES_BY_INDEX: Map<Int, PlacedOre> = FEATURES.associateBy(PlacedOre::globalIndex)
internal val INFESTED_ORE: PlacedOre =
    placed(
        INFESTED_GLOBAL_INDEX,
        fixedCount(14),
        uniformHeight(-64, 63),
        pairedTarget(9, 0.0f, "infested_stone", "infested_deepslate"),
        // parity: the ten biomes that really list `ore_infested`. Only the three windswept ones
        // were here, so a mountain never got a single infested block.
        setOf(
            "cherry_grove",
            "frozen_peaks",
            "grove",
            "jagged_peaks",
            "meadow",
            "snowy_slopes",
            "stony_peaks",
            "windswept_forest",
            "windswept_gravelly_hills",
            "windswept_hills",
        ),
    )
