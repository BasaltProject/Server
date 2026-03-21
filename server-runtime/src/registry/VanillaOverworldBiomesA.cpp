#include <basalt/server/runtime/registry/VanillaOverworldBiomes.hpp>

namespace basalt::server::runtime::registry::vanillaBiomes {

const std::vector<Entry>& overworldBiomesA() {
    static const std::vector<Entry> built{
        Entry{.path = "plains",
              .temperature = 0.8F,
              .downfall = 0.4F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                  }},
        Entry{.path = "sunflower_plains",
              .temperature = 0.8F,
              .downfall = 0.4F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                  }},
        Entry{.path = "snowy_plains",
              .temperature = 0.0F,
              .downfall = 0.5F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7fa1ff")},
                  }},
        Entry{.path = "ice_spikes",
              .temperature = 0.0F,
              .downfall = 0.5F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7fa1ff")},
                  }},
        Entry{.path = "desert",
              .temperature = 2.0F,
              .downfall = 0.0F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.desert", 12000, 24000}}})},
                      {"minecraft:gameplay/snow_golem_melts", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#6eb1ff")},
                  }},
        Entry{
            .path = "swamp",
            .temperature = 0.8F,
            .downfall = 0.9F,
            .hasPrecipitation = true,
            .waterColor = "#617b64",
            .foliageColor = "#6a7039",
            .dryFoliageColor = "#7b5334",
            .grassColorModifier = "swamp",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.overworld.swamp", 12000, 24000}}})},
                    {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                    {"minecraft:visual/water_fog_color", BiomeAttribute::color("#232317")},
                    {"minecraft:visual/water_fog_end_distance", BiomeAttribute::modifier(0.85F, "multiply")},
                }},
        Entry{
            .path = "mangrove_swamp",
            .temperature = 0.8F,
            .downfall = 0.9F,
            .hasPrecipitation = true,
            .waterColor = "#3a7a6a",
            .foliageColor = "#8db127",
            .dryFoliageColor = "#7b5334",
            .grassColorModifier = "swamp",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.overworld.swamp", 12000, 24000}}})},
                    {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                    {"minecraft:visual/fog_color", BiomeAttribute::color("#c0d8ff")},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                    {"minecraft:visual/water_fog_color", BiomeAttribute::color("#4d7a60")},
                    {"minecraft:visual/water_fog_end_distance", BiomeAttribute::modifier(0.85F, "multiply")},
                }},
        Entry{.path = "forest",
              .temperature = 0.7F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.forest", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#79a6ff")},
                  }},
        Entry{.path = "flower_forest",
              .temperature = 0.7F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.flower_forest", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#79a6ff")},
                  }},
        Entry{.path = "birch_forest",
              .temperature = 0.6F,
              .downfall = 0.6F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.forest", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7aa5ff")},
                  }},
        Entry{.path = "dappled_forest",
              .temperature = 0.6F,
              .downfall = 0.6F,
              .hasPrecipitation = true,
              .waterColor = "#375154",
              .grassColor = "#df6827",
              .foliageColor = "#e68e30",
              .dryFoliageColor = "#8c3a04",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.forest", 12000, 24000}}})},
                      {"minecraft:visual/fog_color", BiomeAttribute::color("#ccd8e2")},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7ca3ff")},
                      {"minecraft:visual/water_fog_color", BiomeAttribute::color("#375154")},
                  }},
        Entry{.path = "dark_forest",
              .temperature = 0.7F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .dryFoliageColor = "#7b5334",
              .grassColorModifier = "dark_forest",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.forest", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#79a6ff")},
                  }},
        Entry{.path = "pale_garden",
              .temperature = 0.7F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#76889d",
              .grassColor = "#778272",
              .foliageColor = "#878d76",
              .dryFoliageColor = "#a0a69c",
              .attributes =
                  {
                      {"minecraft:audio/background_music", BiomeAttribute::music({})},
                      {"minecraft:audio/music_volume", BiomeAttribute::real(0.0F)},
                      {"minecraft:visual/fog_color", BiomeAttribute::color("#817770")},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#b9b9b9")},
                      {"minecraft:visual/water_fog_color", BiomeAttribute::color("#556980")},
                  }},
        Entry{.path = "old_growth_birch_forest",
              .temperature = 0.6F,
              .downfall = 0.6F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.forest", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7aa5ff")},
                  }},
        Entry{.path = "old_growth_pine_taiga",
              .temperature = 0.3F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.old_growth_taiga", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7ca3ff")},
                  }},
        Entry{.path = "old_growth_spruce_taiga",
              .temperature = 0.25F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.old_growth_taiga", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7da3ff")},
                  }},
        Entry{.path = "taiga",
              .temperature = 0.25F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7da3ff")},
                  }},
        Entry{.path = "snowy_taiga",
              .temperature = -0.5F,
              .downfall = 0.4F,
              .hasPrecipitation = true,
              .waterColor = "#3d57d6",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#839eff")},
                  }},
        Entry{.path = "savanna",
              .temperature = 2.0F,
              .downfall = 0.0F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:gameplay/snow_golem_melts", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#6eb1ff")},
                  }},
        Entry{.path = "savanna_plateau",
              .temperature = 2.0F,
              .downfall = 0.0F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:gameplay/snow_golem_melts", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#6eb1ff")},
                  }},
        Entry{.path = "windswept_hills",
              .temperature = 0.2F,
              .downfall = 0.3F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7da2ff")},
                  }},
        Entry{.path = "windswept_gravelly_hills",
              .temperature = 0.2F,
              .downfall = 0.3F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7da2ff")},
                  }},
        Entry{.path = "windswept_forest",
              .temperature = 0.2F,
              .downfall = 0.3F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7da2ff")},
                  }},
        Entry{.path = "windswept_savanna",
              .temperature = 2.0F,
              .downfall = 0.0F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:gameplay/snow_golem_melts", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#6eb1ff")},
                  }},
        Entry{.path = "jungle",
              .temperature = 0.95F,
              .downfall = 0.9F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.jungle", 12000, 24000}}})},
                      {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#77a8ff")},
                  }},
        Entry{.path = "sparse_jungle",
              .temperature = 0.95F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.sparse_jungle", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#77a8ff")},
                  }},
        Entry{.path = "bamboo_jungle",
              .temperature = 0.95F,
              .downfall = 0.9F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.bamboo_jungle", 12000, 24000}}})},
                      {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#77a8ff")},
                  }},
        Entry{.path = "badlands",
              .temperature = 2.0F,
              .downfall = 0.0F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .grassColor = "#90814d",
              .foliageColor = "#9e814d",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.badlands", 12000, 24000}}})},
                      {"minecraft:gameplay/snow_golem_melts", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#6eb1ff")},
                  }},
        Entry{.path = "eroded_badlands",
              .temperature = 2.0F,
              .downfall = 0.0F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .grassColor = "#90814d",
              .foliageColor = "#9e814d",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.badlands", 12000, 24000}}})},
                      {"minecraft:gameplay/snow_golem_melts", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#6eb1ff")},
                  }},
        Entry{.path = "wooded_badlands",
              .temperature = 2.0F,
              .downfall = 0.0F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .grassColor = "#90814d",
              .foliageColor = "#9e814d",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.badlands", 12000, 24000}}})},
                      {"minecraft:gameplay/snow_golem_melts", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#6eb1ff")},
                  }},
    };
    return built;
}

} // namespace basalt::server::runtime::registry::vanillaBiomes
