package org.basaltmc.server.runtime.registry

import org.basaltmc.server.runtime.registry.VanillaBiomes.BiomeAttribute
import org.basaltmc.server.runtime.registry.VanillaBiomes.Entry
import org.basaltmc.server.runtime.registry.VanillaBiomes.MusicTrack

/**
 * First half of the overworld biome entries, split out of [VanillaBiomes] for file size.
 * Order matters: it is part of the sequence in [VanillaBiomes.ALL], and that sequence is the
 * network id a chunk's biome palette references. Runs from `plains` through `wooded_badlands`.
 */
internal object VanillaOverworldBiomesA {
    internal val ENTRIES: List<Entry> =
        listOf(
            Entry(
                "plains",
                temperature = 0.8f,
                downfall = 0.4f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
                    ),
            ),
            Entry(
                "sunflower_plains",
                temperature = 0.8f,
                downfall = 0.4f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#78a7ff"),
                    ),
            ),
            Entry(
                "snowy_plains",
                temperature = 0.0f,
                downfall = 0.5f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7fa1ff"),
                    ),
            ),
            Entry(
                "ice_spikes",
                temperature = 0.0f,
                downfall = 0.5f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7fa1ff"),
                    ),
            ),
            Entry(
                "desert",
                temperature = 2.0f,
                downfall = 0.0f,
                hasPrecipitation = false,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.desert", 12000, 24000)),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.swamp", 12000, 24000)),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.swamp", 12000, 24000)),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000)),
                            ),
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#79a6ff"),
                    ),
            ),
            Entry(
                "flower_forest",
                temperature = 0.7f,
                downfall = 0.8f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.flower_forest", 12000, 24000),
                                ),
                            ),
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#79a6ff"),
                    ),
            ),
            Entry(
                "birch_forest",
                temperature = 0.6f,
                downfall = 0.6f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000)),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000)),
                            ),
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
                attributes =
                    mapOf(
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.forest", 12000, 24000)),
                            ),
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7aa5ff"),
                    ),
            ),
            Entry(
                "old_growth_pine_taiga",
                temperature = 0.3f,
                downfall = 0.8f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.old_growth_taiga", 12000, 24000),
                                ),
                            ),
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7ca3ff"),
                    ),
            ),
            Entry(
                "old_growth_spruce_taiga",
                temperature = 0.25f,
                downfall = 0.8f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.old_growth_taiga", 12000, 24000),
                                ),
                            ),
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da3ff"),
                    ),
            ),
            Entry(
                "taiga",
                temperature = 0.25f,
                downfall = 0.8f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da3ff"),
                    ),
            ),
            Entry(
                "snowy_taiga",
                temperature = -0.5f,
                downfall = 0.4f,
                hasPrecipitation = true,
                waterColor = "#3d57d6",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#839eff"),
                    ),
            ),
            Entry(
                "savanna",
                temperature = 2.0f,
                downfall = 0.0f,
                hasPrecipitation = false,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
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
                attributes =
                    mapOf(
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
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da2ff"),
                    ),
            ),
            Entry(
                "windswept_gravelly_hills",
                temperature = 0.2f,
                downfall = 0.3f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da2ff"),
                    ),
            ),
            Entry(
                "windswept_forest",
                temperature = 0.2f,
                downfall = 0.3f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#7da2ff"),
                    ),
            ),
            Entry(
                "windswept_savanna",
                temperature = 2.0f,
                downfall = 0.0f,
                hasPrecipitation = false,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf("default" to MusicTrack("minecraft:music.overworld.jungle", 12000, 24000)),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.sparse_jungle", 12000, 24000),
                                ),
                            ),
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#77a8ff"),
                    ),
            ),
            Entry(
                "bamboo_jungle",
                temperature = 0.95f,
                downfall = 0.9f,
                hasPrecipitation = true,
                waterColor = "#3f76e4",
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.bamboo_jungle", 12000, 24000),
                                ),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.badlands", 12000, 24000),
                                ),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.badlands", 12000, 24000),
                                ),
                            ),
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
                attributes =
                    mapOf(
                        "minecraft:audio/background_music" to
                            BiomeAttribute.Music(
                                mapOf(
                                    "default" to MusicTrack("minecraft:music.overworld.badlands", 12000, 24000),
                                ),
                            ),
                        "minecraft:gameplay/snow_golem_melts" to BiomeAttribute.Flag(true),
                        "minecraft:visual/sky_color" to BiomeAttribute.Color("#6eb1ff"),
                    ),
            ),
        )
}
