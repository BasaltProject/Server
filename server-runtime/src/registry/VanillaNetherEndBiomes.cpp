#include <basalt/server/runtime/registry/VanillaOverworldBiomes.hpp>

#include <optional>
#include <utility>

namespace basalt::server::runtime::registry::vanillaBiomes {
namespace {

[[nodiscard]] Entry netherBiome(std::string path, std::string fogColor, std::string ambientPath, std::string musicPath,
                                std::optional<AmbientParticle> particle = std::nullopt) {
    std::vector<std::pair<std::string, BiomeAttribute>> attributes;
    attributes.emplace_back("minecraft:audio/ambient_sounds",
                            BiomeAttribute::sounds("minecraft:ambient." + ambientPath + ".loop",
                                                   AmbientMood{"minecraft:ambient." + ambientPath + ".mood"},
                                                   AmbientAddition{"minecraft:ambient." + ambientPath + ".additions"}));
    attributes.emplace_back(
        "minecraft:audio/background_music",
        BiomeAttribute::music({{"default", MusicTrack{"minecraft:music.nether." + musicPath, 12000, 24000}}}));
    // The particles go between the music and the fog, not after them: the order is what the codec
    // reads back, so a biome with no particles is not the same shape with a gap in it.
    if (particle.has_value()) {
        attributes.emplace_back("minecraft:visual/ambient_particles", BiomeAttribute::ambientParticles({*particle}));
    }
    attributes.emplace_back("minecraft:visual/fog_color", BiomeAttribute::color(std::move(fogColor)));
    return Entry{.path = std::move(path),
                 .temperature = 2.0F,
                 .downfall = 0.0F,
                 .hasPrecipitation = false,
                 .waterColor = "#3f76e4",
                 .attributes = std::move(attributes)};
}

[[nodiscard]] Entry endBiome(std::string path) {
    return Entry{.path = std::move(path),
                 .temperature = 0.5F,
                 .downfall = 0.5F,
                 .hasPrecipitation = false,
                 .waterColor = "#3f76e4"};
}

} // namespace

const std::vector<Entry>& netherBiomes() {
    static const std::vector<Entry> built{netherBiome("nether_wastes", "#330808", "nether_wastes", "nether_wastes"),
                                          netherBiome("warped_forest", "#1a051a", "warped_forest", "warped_forest",
                                                      AmbientParticle{"minecraft:warped_spore", 0.01428F}),
                                          netherBiome("crimson_forest", "#330303", "crimson_forest", "crimson_forest",
                                                      AmbientParticle{"minecraft:crimson_spore", 0.025F}),
                                          netherBiome("soul_sand_valley", "#1b4745", "soul_sand_valley",
                                                      "soul_sand_valley", AmbientParticle{"minecraft:ash", 0.00625F}),
                                          netherBiome("basalt_deltas", "#685f70", "basalt_deltas", "basalt_deltas",
                                                      AmbientParticle{"minecraft:white_ash", 0.118093334F})};
    return built;
}

const std::vector<Entry>& endBiomes() {
    static const std::vector<Entry> built{endBiome("the_end"), endBiome("end_highlands"), endBiome("end_midlands"),
                                          endBiome("small_end_islands"), endBiome("end_barrens")};
    return built;
}

} // namespace basalt::server::runtime::registry::vanillaBiomes
