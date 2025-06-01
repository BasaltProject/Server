package org.basaltmc.registry

/** A single biome, identified by its vanilla network ID. */
public data class Biome(
    public val id: Identifier,
)

/**
 * Every vanilla 26.2 biome. The original 55 Overworld entries retain their existing IDs so saved
 * chunks keep decoding after Nether, End and the void were added. The remaining entries preserve
 * their relative `BiomeData.bootstrap` order. Biome network IDs are dynamic and synced to each
 * client at login, so only internal consistency between this registry, the synced definitions and
 * chunk palettes is observable on the wire.
 */
public object Biomes {
    public val REGISTRY: Registry<Biome> = Registry()

    public val PLAINS: Biome = register(0, "plains")
    public val SUNFLOWER_PLAINS: Biome = register(1, "sunflower_plains")
    public val SNOWY_PLAINS: Biome = register(2, "snowy_plains")
    public val ICE_SPIKES: Biome = register(3, "ice_spikes")
    public val DESERT: Biome = register(4, "desert")
    public val SWAMP: Biome = register(5, "swamp")
    public val MANGROVE_SWAMP: Biome = register(6, "mangrove_swamp")
    public val FOREST: Biome = register(7, "forest")
    public val FLOWER_FOREST: Biome = register(8, "flower_forest")
    public val BIRCH_FOREST: Biome = register(9, "birch_forest")
    public val DARK_FOREST: Biome = register(10, "dark_forest")
    public val PALE_GARDEN: Biome = register(11, "pale_garden")
    public val OLD_GROWTH_BIRCH_FOREST: Biome = register(12, "old_growth_birch_forest")
    public val OLD_GROWTH_PINE_TAIGA: Biome = register(13, "old_growth_pine_taiga")
    public val OLD_GROWTH_SPRUCE_TAIGA: Biome = register(14, "old_growth_spruce_taiga")
    public val TAIGA: Biome = register(15, "taiga")
    public val SNOWY_TAIGA: Biome = register(16, "snowy_taiga")
    public val SAVANNA: Biome = register(17, "savanna")
    public val SAVANNA_PLATEAU: Biome = register(18, "savanna_plateau")
    public val WINDSWEPT_HILLS: Biome = register(19, "windswept_hills")
    public val WINDSWEPT_GRAVELLY_HILLS: Biome = register(20, "windswept_gravelly_hills")
    public val WINDSWEPT_FOREST: Biome = register(21, "windswept_forest")
    public val WINDSWEPT_SAVANNA: Biome = register(22, "windswept_savanna")
    public val JUNGLE: Biome = register(23, "jungle")
    public val SPARSE_JUNGLE: Biome = register(24, "sparse_jungle")
    public val BAMBOO_JUNGLE: Biome = register(25, "bamboo_jungle")
    public val BADLANDS: Biome = register(26, "badlands")
    public val ERODED_BADLANDS: Biome = register(27, "eroded_badlands")
    public val WOODED_BADLANDS: Biome = register(28, "wooded_badlands")
    public val MEADOW: Biome = register(29, "meadow")
    public val CHERRY_GROVE: Biome = register(30, "cherry_grove")
    public val GROVE: Biome = register(31, "grove")
    public val SNOWY_SLOPES: Biome = register(32, "snowy_slopes")
    public val FROZEN_PEAKS: Biome = register(33, "frozen_peaks")
    public val JAGGED_PEAKS: Biome = register(34, "jagged_peaks")
    public val STONY_PEAKS: Biome = register(35, "stony_peaks")
    public val RIVER: Biome = register(36, "river")
    public val FROZEN_RIVER: Biome = register(37, "frozen_river")
    public val BEACH: Biome = register(38, "beach")
    public val SNOWY_BEACH: Biome = register(39, "snowy_beach")
    public val STONY_SHORE: Biome = register(40, "stony_shore")
    public val WARM_OCEAN: Biome = register(41, "warm_ocean")
    public val LUKEWARM_OCEAN: Biome = register(42, "lukewarm_ocean")
    public val DEEP_LUKEWARM_OCEAN: Biome = register(43, "deep_lukewarm_ocean")
    public val OCEAN: Biome = register(44, "ocean")
    public val DEEP_OCEAN: Biome = register(45, "deep_ocean")
    public val COLD_OCEAN: Biome = register(46, "cold_ocean")
    public val DEEP_COLD_OCEAN: Biome = register(47, "deep_cold_ocean")
    public val FROZEN_OCEAN: Biome = register(48, "frozen_ocean")
    public val DEEP_FROZEN_OCEAN: Biome = register(49, "deep_frozen_ocean")
    public val MUSHROOM_FIELDS: Biome = register(50, "mushroom_fields")
    public val DRIPSTONE_CAVES: Biome = register(51, "dripstone_caves")
    public val LUSH_CAVES: Biome = register(52, "lush_caves")
    public val DEEP_DARK: Biome = register(53, "deep_dark")
    public val SULFUR_CAVES: Biome = register(54, "sulfur_caves")

    public val THE_VOID: Biome = register(55, "the_void")

    public val NETHER_WASTES: Biome = register(56, "nether_wastes")
    public val WARPED_FOREST: Biome = register(57, "warped_forest")
    public val CRIMSON_FOREST: Biome = register(58, "crimson_forest")
    public val SOUL_SAND_VALLEY: Biome = register(59, "soul_sand_valley")
    public val BASALT_DELTAS: Biome = register(60, "basalt_deltas")

    public val THE_END: Biome = register(61, "the_end")
    public val END_HIGHLANDS: Biome = register(62, "end_highlands")
    public val END_MIDLANDS: Biome = register(63, "end_midlands")
    public val SMALL_END_ISLANDS: Biome = register(64, "small_end_islands")
    public val END_BARRENS: Biome = register(65, "end_barrens")

    private fun register(
        id: Int,
        path: String,
    ): Biome {
        val identifier = Identifier.vanilla(path)
        return REGISTRY.register(id, identifier, Biome(identifier))
    }
}
