#include <basalt/server/runtime/registry/VanillaOverworldBiomes.hpp>

namespace basalt::server::runtime::registry::vanillaBiomes {

const std::vector<Entry>& overworldBiomesB() {
    static const std::vector<Entry> built{
        Entry{.path = "meadow",
              .temperature = 0.5F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#0e4ecf",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.meadow", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                  }},
        Entry{.path = "cherry_grove",
              .temperature = 0.5F,
              .downfall = 0.8F,
              .hasPrecipitation = true,
              .waterColor = "#5db7ef",
              .grassColor = "#b6db61",
              .foliageColor = "#b6db61",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.cherry_grove", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                      {"minecraft:visual/water_fog_color", BiomeAttribute::color("#5db7ef")},
                  }},
        Entry{
            .path = "grove",
            .temperature = -0.2F,
            .downfall = 0.8F,
            .hasPrecipitation = true,
            .waterColor = "#3f76e4",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.overworld.grove", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#81a0ff")},
                }},
        Entry{.path = "snowy_slopes",
              .temperature = -0.3F,
              .downfall = 0.9F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.snowy_slopes", 12000, 24000}}})},
                      {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#829fff")},
                  }},
        Entry{.path = "frozen_peaks",
              .temperature = -0.7F,
              .downfall = 0.9F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.frozen_peaks", 12000, 24000}}})},
                      {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#859dff")},
                  }},
        Entry{.path = "jagged_peaks",
              .temperature = -0.7F,
              .downfall = 0.9F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.jagged_peaks", 12000, 24000}}})},
                      {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#859dff")},
                  }},
        Entry{.path = "stony_peaks",
              .temperature = 1.0F,
              .downfall = 0.3F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.stony_peaks", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#76a8ff")},
                  }},
        Entry{
            .path = "river",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#3f76e4",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                }},
        Entry{
            .path = "frozen_river",
            .temperature = 0.0F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#3938c9",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7fa1ff")},
                }},
        Entry{.path = "beach",
              .temperature = 0.8F,
              .downfall = 0.4F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                  }},
        Entry{.path = "snowy_beach",
              .temperature = 0.05F,
              .downfall = 0.3F,
              .hasPrecipitation = true,
              .waterColor = "#3d57d6",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7fa1ff")},
                  }},
        Entry{.path = "stony_shore",
              .temperature = 0.2F,
              .downfall = 0.3F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7da2ff")},
                  }},
        Entry{
            .path = "warm_ocean",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#43d5ee",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                    {"minecraft:visual/water_fog_color", BiomeAttribute::color("#041f33")},
                }},
        Entry{
            .path = "lukewarm_ocean",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#45adf2",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                    {"minecraft:visual/water_fog_color", BiomeAttribute::color("#041633")},
                }},
        Entry{
            .path = "deep_lukewarm_ocean",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#45adf2",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                    {"minecraft:visual/water_fog_color", BiomeAttribute::color("#041633")},
                }},
        Entry{
            .path = "ocean",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#3f76e4",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                }},
        Entry{
            .path = "deep_ocean",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#3f76e4",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                }},
        Entry{
            .path = "cold_ocean",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#3d57d6",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                }},
        Entry{
            .path = "deep_cold_ocean",
            .temperature = 0.5F,
            .downfall = 0.5F,
            .hasPrecipitation = true,
            .waterColor = "#3d57d6",
            .attributes =
                {
                    {"minecraft:audio/background_music",
                     BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.game", 12000, 24000}},
                                            {"creative", MusicTrack{"minecraft:music.creative", 12000, 24000}},
                                            {"underwater", MusicTrack{"minecraft:music.under_water", 12000, 24000}}})},
                    {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                }},
        Entry{.path = "frozen_ocean",
              .temperature = 0.0F,
              .downfall = 0.5F,
              .hasPrecipitation = true,
              .waterColor = "#3938c9",
              .temperatureModifier = "frozen",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7fa1ff")},
                  }},
        Entry{.path = "deep_frozen_ocean",
              .temperature = 0.5F,
              .downfall = 0.5F,
              .hasPrecipitation = true,
              .waterColor = "#3938c9",
              .temperatureModifier = "frozen",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                  }},
        Entry{.path = "mushroom_fields",
              .temperature = 0.9F,
              .downfall = 1.0F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:gameplay/can_pillager_patrol_spawn", BiomeAttribute::boolean(false)},
                      {"minecraft:gameplay/increased_fire_burnout", BiomeAttribute::boolean(true)},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#77a8ff")},
                  }},
        Entry{.path = "dripstone_caves",
              .temperature = 0.8F,
              .downfall = 0.4F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.dripstone_caves", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                  }},
        Entry{.path = "lush_caves",
              .temperature = 0.5F,
              .downfall = 0.5F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.lush_caves", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                  }},
        Entry{.path = "deep_dark",
              .temperature = 0.8F,
              .downfall = 0.4F,
              .hasPrecipitation = true,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.deep_dark", 12000, 24000}}})},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                  }},
        Entry{.path = "sulfur_caves",
              .temperature = 0.8F,
              .downfall = 0.4F,
              .hasPrecipitation = true,
              .waterColor = "#34bf89",
              .grassColor = "#aba64f",
              .attributes =
                  {
                      {"minecraft:audio/background_music",
                       BiomeAttribute::music(
                           {{"default", MusicTrack{"minecraft:music.overworld.sulfur_caves", 12000, 24000}}})},
                      {"minecraft:visual/fog_color", BiomeAttribute::color("#8cb831")},
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#78a7ff")},
                      {"minecraft:visual/water_fog_color", BiomeAttribute::color("#17543c")},
                  }},
        Entry{.path = "the_void",
              .temperature = 0.5F,
              .downfall = 0.5F,
              .hasPrecipitation = false,
              .waterColor = "#3f76e4",
              .attributes =
                  {
                      {"minecraft:visual/sky_color", BiomeAttribute::color("#7ba4ff")},
                  }},
    };
    return built;
}

} // namespace basalt::server::runtime::registry::vanillaBiomes
