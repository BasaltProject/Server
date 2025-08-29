package org.basaltmc.server.runtime.world

/** Pure Minecraft 26.2 placement tables shared by runtime and chunk-generation spawning. */
internal object NaturalSpawnPredicates {
    /** `NO_RESTRICTIONS` registrations relevant to natural CREATURE biome lists. */
    fun usesGroundPlacement(type: String?): Boolean = type !in NO_GROUND_PLACEMENT

    /** The concrete block/biome predicates called by the registered animal spawn rule. */
    fun allows(
        type: String?,
        ground: String,
        groundIsLeaves: Boolean,
        biome: String?,
        y: Int,
        seaLevel: Int,
    ): Boolean =
        when (type) {
            "armadillo" -> ground in ARMADILLO_GROUND
            "camel" -> ground in SAND_GROUND
            "fox", "wolf" -> ground in FOX_WOLF_GROUND
            "frog" -> ground in FROG_GROUND
            "goat" -> ground in GOAT_GROUND
            "mooshroom" -> ground == "mycelium"
            "parrot" ->
                ground == "grass_block" ||
                    ground == "air" ||
                    ground.endsWith("_log") ||
                    ground.endsWith("_wood") ||
                    ground in NETHER_STEMS ||
                    groundIsLeaves
            "polar_bear" ->
                if (biome in POLAR_BEAR_ALTERNATE_BIOMES) {
                    ground == "ice"
                } else {
                    ground == "grass_block"
                }
            "rabbit" -> ground in RABBIT_GROUND
            "turtle" -> y < seaLevel + 4 && ground in SAND_GROUND
            else -> ground == "grass_block"
        }

    /** `Monster.checkSurfaceMonstersSpawnRules` registrations present in natural biome lists. */
    fun isSurfaceMonster(type: String?): Boolean = type in SURFACE_MONSTERS

    /** `SpawnPlacementTypes.IN_WATER`: water here and no redstone conductor immediately above. */
    fun waterPlacementAllows(
        atHasWater: Boolean,
        aboveConductsRedstone: Boolean,
    ): Boolean = atHasWater && !aboveConductsRedstone

    /** Glow squid additionally requires the block itself, not merely its fluid, to be water. */
    fun glowSquidAllows(
        y: Int,
        seaLevel: Int,
        rawBrightness: Int,
        atIsWaterBlock: Boolean,
    ): Boolean = y <= seaLevel - 33 && rawBrightness == 0 && atIsWaterBlock

    /** The exact short-circuit order of `Guardian.checkGuardianSpawnRules`. */
    fun guardianAllows(
        oneInTwentyRoll: Int,
        difficulty: String,
        atHasWater: Boolean,
        belowHasWater: Boolean,
        canSeeSkyFromBelowWater: () -> Boolean,
    ): Boolean =
        (oneInTwentyRoll == 0 || !canSeeSkyFromBelowWater()) &&
            difficulty != "peaceful" &&
            atHasWater &&
            belowHasWater

    private val NO_GROUND_PLACEMENT = setOf("fox", "panda", "trader_llama")

    private val ARMADILLO_GROUND =
        setOf(
            "grass_block",
            "red_sand",
            "coarse_dirt",
            "terracotta",
            "white_terracotta",
            "yellow_terracotta",
            "orange_terracotta",
            "red_terracotta",
            "brown_terracotta",
            "light_gray_terracotta",
        )
    private val SAND_GROUND = setOf("sand", "red_sand", "suspicious_sand")
    private val FOX_WOLF_GROUND = setOf("grass_block", "snow", "snow_block", "podzol", "coarse_dirt")
    private val FROG_GROUND = setOf("grass_block", "mud", "mangrove_roots", "muddy_mangrove_roots")
    private val GOAT_GROUND = setOf("grass_block", "stone", "snow", "snow_block", "packed_ice", "gravel")
    private val RABBIT_GROUND = setOf("grass_block", "snow", "snow_block", "sand")
    private val NETHER_STEMS =
        setOf(
            "crimson_stem",
            "stripped_crimson_stem",
            "crimson_hyphae",
            "stripped_crimson_hyphae",
            "warped_stem",
            "stripped_warped_stem",
            "warped_hyphae",
            "stripped_warped_hyphae",
        )
    private val POLAR_BEAR_ALTERNATE_BIOMES = setOf("frozen_ocean", "deep_frozen_ocean")
    private val SURFACE_MONSTERS = setOf("camel_husk", "husk", "parched", "stray")
}
