package org.basaltmc.world.terrain

/**
 * // parity: the `.temperature(...)`/`.temperatureAdjustment(TemperatureModifier.FROZEN)` calls in
 * the real jar's `net.minecraft.data.worldgen.biome.OverworldBiomes`/`BiomeData.bootstrap`
 * (decompiled): every Overworld biome's real base temperature, which two (`frozen_ocean`,
 * `deep_frozen_ocean`) get the `FROZEN` modifier, and whether precipitation is enabled. Downfall
 * remains omitted because neither temperature nor the top-layer freezing feature reads it.
 */
public data class BiomeClimate(
    val temperature: Float,
    val frozen: Boolean = false,
    val hasPrecipitation: Boolean = true,
)

public object BiomeClimateData {
    private val byPath: Map<String, BiomeClimate> =
        mapOf(
            "plains" to BiomeClimate(0.8f),
            "sunflower_plains" to BiomeClimate(0.8f),
            "snowy_plains" to BiomeClimate(0.0f),
            "ice_spikes" to BiomeClimate(0.0f),
            "desert" to BiomeClimate(2.0f, hasPrecipitation = false),
            "swamp" to BiomeClimate(0.8f),
            "mangrove_swamp" to BiomeClimate(0.8f),
            "forest" to BiomeClimate(0.7f),
            "flower_forest" to BiomeClimate(0.7f),
            "birch_forest" to BiomeClimate(0.6f),
            "dark_forest" to BiomeClimate(0.7f),
            "pale_garden" to BiomeClimate(0.7f),
            "old_growth_birch_forest" to BiomeClimate(0.6f),
            "old_growth_pine_taiga" to BiomeClimate(0.3f),
            "old_growth_spruce_taiga" to BiomeClimate(0.25f),
            "taiga" to BiomeClimate(0.25f),
            "snowy_taiga" to BiomeClimate(-0.5f),
            "savanna" to BiomeClimate(2.0f, hasPrecipitation = false),
            "savanna_plateau" to BiomeClimate(2.0f, hasPrecipitation = false),
            "windswept_hills" to BiomeClimate(0.2f),
            "windswept_gravelly_hills" to BiomeClimate(0.2f),
            "windswept_forest" to BiomeClimate(0.2f),
            "windswept_savanna" to BiomeClimate(2.0f, hasPrecipitation = false),
            "jungle" to BiomeClimate(0.95f),
            "sparse_jungle" to BiomeClimate(0.95f),
            "bamboo_jungle" to BiomeClimate(0.95f),
            "badlands" to BiomeClimate(2.0f, hasPrecipitation = false),
            "eroded_badlands" to BiomeClimate(2.0f, hasPrecipitation = false),
            "wooded_badlands" to BiomeClimate(2.0f, hasPrecipitation = false),
            "meadow" to BiomeClimate(0.5f),
            "cherry_grove" to BiomeClimate(0.5f),
            "grove" to BiomeClimate(-0.2f),
            "snowy_slopes" to BiomeClimate(-0.3f),
            "frozen_peaks" to BiomeClimate(-0.7f),
            "jagged_peaks" to BiomeClimate(-0.7f),
            "stony_peaks" to BiomeClimate(1.0f),
            "river" to BiomeClimate(0.5f),
            "frozen_river" to BiomeClimate(0.0f),
            "beach" to BiomeClimate(0.8f),
            "snowy_beach" to BiomeClimate(0.05f),
            "stony_shore" to BiomeClimate(0.2f),
            "warm_ocean" to BiomeClimate(0.5f),
            "lukewarm_ocean" to BiomeClimate(0.5f),
            "deep_lukewarm_ocean" to BiomeClimate(0.5f),
            "ocean" to BiomeClimate(0.5f),
            "deep_ocean" to BiomeClimate(0.5f),
            "cold_ocean" to BiomeClimate(0.5f),
            "deep_cold_ocean" to BiomeClimate(0.5f),
            "frozen_ocean" to BiomeClimate(0.0f, frozen = true),
            "deep_frozen_ocean" to BiomeClimate(0.5f, frozen = true),
            "mushroom_fields" to BiomeClimate(0.9f),
            "dripstone_caves" to BiomeClimate(0.8f),
            "lush_caves" to BiomeClimate(0.5f),
            "deep_dark" to BiomeClimate(0.8f),
            "sulfur_caves" to BiomeClimate(0.8f),
        )

    /** [id] is a full `namespace:path` identifier string (see [org.basaltmc.registry.Identifier.toString]); unknown biomes fall back to `0.5`, vanilla's own `Biome` default. */
    public fun forBiomeId(id: String): BiomeClimate = byPath[id.substringAfter(':')] ?: BiomeClimate(0.5f)
}
