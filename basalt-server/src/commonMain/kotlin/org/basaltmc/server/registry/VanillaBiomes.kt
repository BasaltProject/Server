package org.basaltmc.server.registry

import org.basaltmc.nbt.NbtCompound

/**
 * The 55 real Overworld biome definitions exactly as a vanilla 26.2 server syncs them in the
 * `minecraft:worldgen/biome` registry during Configuration, built from the real jar's own
 * `data/minecraft/worldgen/biome/` JSON files (only the fields the real `Biome.NETWORK_CODEC` carries:
 * climate settings, the environment attribute map and the special effects; spawners/features/
 * carvers are server-side only and never synced). Entry order matches [org.basaltmc.registry.Biomes]
 * (the real `BiomeData.bootstrap` order): the registry's numeric id of each entry is its index
 * here, which is what a chunk's biome palette then references.
 *
 * Color attributes and effect colors serialize as `#rrggbb` strings, the real
 * `ExtraCodecs.STRING_RGB_COLOR`'s own serialized form (it also decodes plain ints, which is why
 * the earlier int-encoded `plainsBiome` worked, but a vanilla server sends the string form).
 */
public object VanillaBiomes {
    /** One environment attribute value, in the shapes the real Overworld biome data actually uses. */
    public sealed interface BiomeAttribute {
        public data class Color(
            val hex: String,
        ) : BiomeAttribute

        public data class Flag(
            val value: Boolean,
        ) : BiomeAttribute

        public data class Number(
            val value: Float,
        ) : BiomeAttribute

        /** A real attribute modifier object (`{"argument": ..., "modifier": "multiply"}`), e.g. `visual/water_fog_end_distance`. */
        public data class Modifier(
            val argument: Float,
            val modifier: String,
        ) : BiomeAttribute

        /** `audio/background_music`: per-slot tracks (`default`/`creative`/`underwater`). */
        public data class Music(
            val tracks: Map<String, MusicTrack>,
        ) : BiomeAttribute
    }

    public data class MusicTrack(
        val sound: String,
        val minDelay: Int,
        val maxDelay: Int,
    )

    public data class Entry(
        val path: String,
        val temperature: Float,
        val downfall: Float,
        val hasPrecipitation: Boolean,
        val waterColor: String,
        val temperatureModifier: String? = null,
        val grassColor: String? = null,
        val foliageColor: String? = null,
        val dryFoliageColor: String? = null,
        val grassColorModifier: String? = null,
        val attributes: Map<String, BiomeAttribute> = emptyMap(),
    )

    /** All 55 Overworld biomes, in [org.basaltmc.registry.Biomes] registration order. */
    public val ALL: List<Entry> =
        listOf(
        Entry(
            "plains",
            temperature = 0.8f,
            downfall = 0.4f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
            ),
        ),
        Entry(
            "sunflower_plains",
            temperature = 0.8f,
            downfall = 0.4f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
            ),
        ),
        Entry(
            "snowy_plains",
            temperature = 0.0f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7fa1ff"),
            ),
        ),
        Entry(
            "ice_spikes",
            temperature = 0.0f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7fa1ff"),
            ),
        ),
        Entry(
            "desert",
            temperature = 2.0f,
            downfall = 0.0f,
            hasPrecipitation = false,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.desert", 12000, 24000))),
                "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
            ),
        ),
        Entry(
            "swamp",
            temperature = 0.8f,
            downfall = 0.9f,
            hasPrecipitation = true,
            waterColor = "#617b64",
            foliageColor = "#6a7039",
            dryFoliageColor = "#7b5334",
            grassColorModifier = "swamp",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.swamp", 12000, 24000))),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#232317"),
                "minecraft:visual/water_fog_end_distance" to BiomeAttribute.Modifier(0.85f, "multiply"),
            ),
        ),
        Entry(
            "mangrove_swamp",
            temperature = 0.8f,
            downfall = 0.9f,
            hasPrecipitation = true,
            waterColor = "#3a7a6a",
            foliageColor = "#8db127",
            dryFoliageColor = "#7b5334",
            grassColorModifier = "swamp",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.swamp", 12000, 24000))),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/fog_color" to BiomeAttribute.Color("#c0d8ff"),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#4d7a60"),
                "minecraft:visual/water_fog_end_distance" to BiomeAttribute.Modifier(0.85f, "multiply"),
            ),
        ),
        Entry(
            "forest",
            temperature = 0.7f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#79a6ff"),
            ),
        ),
        Entry(
            "flower_forest",
            temperature = 0.7f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.flower_forest", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#79a6ff"),
            ),
        ),
        Entry(
            "birch_forest",
            temperature = 0.6f,
            downfall = 0.6f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7aa5ff"),
            ),
        ),
        Entry(
            "dark_forest",
            temperature = 0.7f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            dryFoliageColor = "#7b5334",
            grassColorModifier = "dark_forest",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#79a6ff"),
            ),
        ),
        Entry(
            "pale_garden",
            temperature = 0.7f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#76889d",
            grassColor = "#778272",
            foliageColor = "#878d76",
            dryFoliageColor = "#a0a69c",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf()),
                "minecraft:audio/music_volume" to BiomeAttribute.Number(0.0f),
                "minecraft:visual/fog_color" to BiomeAttribute.Color("#817770"),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#b9b9b9"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#556980"),
            ),
        ),
        Entry(
            "old_growth_birch_forest",
            temperature = 0.6f,
            downfall = 0.6f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7aa5ff"),
            ),
        ),
        Entry(
            "old_growth_pine_taiga",
            temperature = 0.3f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.old_growth_taiga", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ca3ff"),
            ),
        ),
        Entry(
            "old_growth_spruce_taiga",
            temperature = 0.25f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.old_growth_taiga", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da3ff"),
            ),
        ),
        Entry(
            "taiga",
            temperature = 0.25f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da3ff"),
            ),
        ),
        Entry(
            "snowy_taiga",
            temperature = -0.5f,
            downfall = 0.4f,
            hasPrecipitation = true,
            waterColor = "#3d57d6",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#839eff"),
            ),
        ),
        Entry(
            "savanna",
            temperature = 2.0f,
            downfall = 0.0f,
            hasPrecipitation = false,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
            ),
        ),
        Entry(
            "savanna_plateau",
            temperature = 2.0f,
            downfall = 0.0f,
            hasPrecipitation = false,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
            ),
        ),
        Entry(
            "windswept_hills",
            temperature = 0.2f,
            downfall = 0.3f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da2ff"),
            ),
        ),
        Entry(
            "windswept_gravelly_hills",
            temperature = 0.2f,
            downfall = 0.3f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da2ff"),
            ),
        ),
        Entry(
            "windswept_forest",
            temperature = 0.2f,
            downfall = 0.3f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da2ff"),
            ),
        ),
        Entry(
            "windswept_savanna",
            temperature = 2.0f,
            downfall = 0.0f,
            hasPrecipitation = false,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
            ),
        ),
        Entry(
            "jungle",
            temperature = 0.95f,
            downfall = 0.9f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.jungle", 12000, 24000))),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#77a8ff"),
            ),
        ),
        Entry(
            "sparse_jungle",
            temperature = 0.95f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.sparse_jungle", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#77a8ff"),
            ),
        ),
        Entry(
            "bamboo_jungle",
            temperature = 0.95f,
            downfall = 0.9f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.bamboo_jungle", 12000, 24000))),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#77a8ff"),
            ),
        ),
        Entry(
            "badlands",
            temperature = 2.0f,
            downfall = 0.0f,
            hasPrecipitation = false,
            waterColor = "#3f76e4",
            grassColor = "#90814d",
            foliageColor = "#9e814d",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.badlands", 12000, 24000))),
                "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
            ),
        ),
        Entry(
            "eroded_badlands",
            temperature = 2.0f,
            downfall = 0.0f,
            hasPrecipitation = false,
            waterColor = "#3f76e4",
            grassColor = "#90814d",
            foliageColor = "#9e814d",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.badlands", 12000, 24000))),
                "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
            ),
        ),
        Entry(
            "wooded_badlands",
            temperature = 2.0f,
            downfall = 0.0f,
            hasPrecipitation = false,
            waterColor = "#3f76e4",
            grassColor = "#90814d",
            foliageColor = "#9e814d",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.badlands", 12000, 24000))),
                "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
            ),
        ),
        Entry(
            "meadow",
            temperature = 0.5f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#0e4ecf",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.meadow", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "cherry_grove",
            temperature = 0.5f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#5db7ef",
            grassColor = "#b6db61",
            foliageColor = "#b6db61",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.cherry_grove", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#5db7ef"),
            ),
        ),
        Entry(
            "grove",
            temperature = -0.2f,
            downfall = 0.8f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.grove", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#81a0ff"),
            ),
        ),
        Entry(
            "snowy_slopes",
            temperature = -0.3f,
            downfall = 0.9f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.snowy_slopes", 12000, 24000))),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#829fff"),
            ),
        ),
        Entry(
            "frozen_peaks",
            temperature = -0.7f,
            downfall = 0.9f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.frozen_peaks", 12000, 24000))),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#859dff"),
            ),
        ),
        Entry(
            "jagged_peaks",
            temperature = -0.7f,
            downfall = 0.9f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.jagged_peaks", 12000, 24000))),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#859dff"),
            ),
        ),
        Entry(
            "stony_peaks",
            temperature = 1.0f,
            downfall = 0.3f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.stony_peaks", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#76a8ff"),
            ),
        ),
        Entry(
            "river",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "frozen_river",
            temperature = 0.0f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3938c9",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7fa1ff"),
            ),
        ),
        Entry(
            "beach",
            temperature = 0.8f,
            downfall = 0.4f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
            ),
        ),
        Entry(
            "snowy_beach",
            temperature = 0.05f,
            downfall = 0.3f,
            hasPrecipitation = true,
            waterColor = "#3d57d6",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7fa1ff"),
            ),
        ),
        Entry(
            "stony_shore",
            temperature = 0.2f,
            downfall = 0.3f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da2ff"),
            ),
        ),
        Entry(
            "warm_ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#43d5ee",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#041f33"),
            ),
        ),
        Entry(
            "lukewarm_ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#45adf2",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#041633"),
            ),
        ),
        Entry(
            "deep_lukewarm_ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#45adf2",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#041633"),
            ),
        ),
        Entry(
            "ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "deep_ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "cold_ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3d57d6",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "deep_cold_ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3d57d6",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.game", 12000, 24000), "creative" to MusicTrack("minecraft:music.creative", 12000, 24000), "underwater" to MusicTrack("minecraft:music.under_water", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "frozen_ocean",
            temperature = 0.0f,
            downfall = 0.5f,
            hasPrecipitation = true,
            temperatureModifier = "frozen",
            waterColor = "#3938c9",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7fa1ff"),
            ),
        ),
        Entry(
            "deep_frozen_ocean",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            temperatureModifier = "frozen",
            waterColor = "#3938c9",
            attributes = mapOf(
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "mushroom_fields",
            temperature = 0.9f,
            downfall = 1.0f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:gameplay/can_pillager_patrol_spawn" to BiomeAttribute.Flag(false),
                "minecraft:gameplay/increased_fire_burnout" to BiomeAttribute.Flag(true),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#77a8ff"),
            ),
        ),
        Entry(
            "dripstone_caves",
            temperature = 0.8f,
            downfall = 0.4f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.dripstone_caves", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
            ),
        ),
        Entry(
            "lush_caves",
            temperature = 0.5f,
            downfall = 0.5f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.lush_caves", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ba4ff"),
            ),
        ),
        Entry(
            "deep_dark",
            temperature = 0.8f,
            downfall = 0.4f,
            hasPrecipitation = true,
            waterColor = "#3f76e4",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.deep_dark", 12000, 24000))),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
            ),
        ),
        Entry(
            "sulfur_caves",
            temperature = 0.8f,
            downfall = 0.4f,
            hasPrecipitation = true,
            waterColor = "#34bf89",
            grassColor = "#aba64f",
            attributes = mapOf(
                "minecraft:audio/background_music" to BiomeAttribute.Music(mapOf("default" to MusicTrack("minecraft:music.overworld.sulfur_caves", 12000, 24000))),
                "minecraft:visual/fog_color" to BiomeAttribute.Color("#8cb831"),
                "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
                "minecraft:visual/water_fog_color" to BiomeAttribute.Color("#17543c"),
            ),
        ),        )

    /** Serializes one entry to the exact NBT a vanilla server sends for it in `RegistryDataPacket`. */
    public fun Entry.toNbt(): NbtCompound =
        NbtCompound().apply {
            putBoolean("has_precipitation", hasPrecipitation)
            putFloat("temperature", temperature)
            temperatureModifier?.let { putString("temperature_modifier", it) }
            putFloat("downfall", downfall)
            if (attributes.isNotEmpty()) {
                this["attributes"] =
                    NbtCompound().apply {
                        for ((key, value) in attributes) {
                            when (value) {
                                is BiomeAttribute.Color -> putString(key, value.hex)
                                is BiomeAttribute.Flag -> putBoolean(key, value.value)
                                is BiomeAttribute.Number -> putFloat(key, value.value)
                                is BiomeAttribute.Modifier ->
                                    this[key] =
                                        NbtCompound().apply {
                                            putFloat("argument", value.argument)
                                            putString("modifier", value.modifier)
                                        }
                                is BiomeAttribute.Music ->
                                    this[key] =
                                        NbtCompound().apply {
                                            for ((slot, track) in value.tracks) {
                                                this[slot] =
                                                    NbtCompound().apply {
                                                        putString("sound", track.sound)
                                                        putInt("min_delay", track.minDelay)
                                                        putInt("max_delay", track.maxDelay)
                                                    }
                                            }
                                        }
                            }
                        }
                    }
            }
            this["effects"] =
                NbtCompound().apply {
                    putString("water_color", waterColor)
                    foliageColor?.let { putString("foliage_color", it) }
                    dryFoliageColor?.let { putString("dry_foliage_color", it) }
                    grassColor?.let { putString("grass_color", it) }
                    grassColorModifier?.let { putString("grass_color_modifier", it) }
                }
        }
}
