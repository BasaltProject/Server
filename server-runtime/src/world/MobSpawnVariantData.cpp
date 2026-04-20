#include <basalt/server/runtime/world/MobSpawnVariantData.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

constexpr std::array<std::pair<std::string_view, std::int32_t>, 7> kGenes{
    {{"normal", 0}, {"lazy", 1}, {"worried", 2}, {"playful", 3}, {"brown", 4}, {"weak", 5}, {"aggressive", 6}}};

} // namespace

std::string_view pandaGeneName(PandaGene gene) noexcept { return kGenes[static_cast<std::size_t>(gene)].first; }

std::int32_t pandaGeneId(PandaGene gene) noexcept { return kGenes[static_cast<std::size_t>(gene)].second; }

bool pandaGeneRecessive(PandaGene gene) noexcept { return gene == PandaGene::Brown || gene == PandaGene::Weak; }

std::optional<PandaGene> pandaGeneByName(std::string_view name) {
    for (std::size_t index = 0; index < kGenes.size(); index++) {
        if (kGenes[index].first == name) {
            return static_cast<PandaGene>(index);
        }
    }
    return std::nullopt;
}

namespace mobSpawnVariantData {

const std::vector<std::string>& chickenSoundVariants() {
    static const std::vector<std::string> all{"classic", "picky"};
    return all;
}

const std::vector<std::string>& cowSoundVariants() {
    static const std::vector<std::string> all{"classic", "moody"};
    return all;
}

const std::vector<std::string>& pigSoundVariants() {
    static const std::vector<std::string> all{"classic", "big", "mini"};
    return all;
}

const std::vector<std::string>& wolfVariants() {
    static const std::vector<std::string> all{"pale",  "spotted", "snowy",    "black",  "ashen",
                                              "rusty", "woods",   "chestnut", "striped"};
    return all;
}

const std::vector<std::string>& wolfSoundVariants() {
    static const std::vector<std::string> all{"classic", "puglin", "sad", "angry", "grumpy", "big", "cute"};
    return all;
}

const std::vector<std::int32_t>& tropicalCommonVariants() {
    // parity: `TropicalFish.COMMON_VARIANTS`.
    static const std::vector<std::int32_t> all{117506305, 117899265, 185008129, 117441793, 118161664, 65536,
                                               50726144,  67764993,  234882305, 67110144,  117441025, 16778497,
                                               101253888, 50660352,  918529,    235340288, 918273,    67108865,
                                               917504,    459008,    67699456,  67371009};
    return all;
}

const std::set<std::string, std::less<>>& whiteRabbitBiomes() {
    static const std::set<std::string, std::less<>> all{"frozen_ocean", "frozen_peaks", "frozen_river", "grove",
                                                        "ice_spikes",   "jagged_peaks", "snowy_beach",  "snowy_plains",
                                                        "snowy_slopes", "snowy_taiga"};
    return all;
}

const std::set<std::string, std::less<>>& warmFarmBiomes() {
    static const std::set<std::string, std::less<>> all{"badlands",
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
                                                        "wooded_badlands"};
    return all;
}

const std::set<std::string, std::less<>>& coldFarmBiomes() {
    static const std::set<std::string, std::less<>> all{"cold_ocean",
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
                                                        "windswept_hills"};
    return all;
}

const std::set<std::string, std::less<>>& warmFrogBiomes() {
    static const std::set<std::string, std::less<>> all{
        "badlands",      "bamboo_jungle",  "basalt_deltas", "crimson_forest",    "desert",          "eroded_badlands",
        "jungle",        "mangrove_swamp", "nether_wastes", "savanna",           "savanna_plateau", "soul_sand_valley",
        "sparse_jungle", "warm_ocean",     "warped_forest", "windswept_savanna", "wooded_badlands"};
    return all;
}

const std::set<std::string, std::less<>>& coldFrogBiomes() {
    static const std::set<std::string, std::less<>> all{
        "deep_dark",    "deep_frozen_ocean", "end_barrens",  "end_highlands", "end_midlands", "frozen_ocean",
        "frozen_peaks", "frozen_river",      "grove",        "ice_spikes",    "jagged_peaks", "small_end_islands",
        "snowy_beach",  "snowy_plains",      "snowy_slopes", "snowy_taiga",   "the_end"};
    return all;
}

const std::set<std::string, std::less<>>& savannaBiomes() {
    static const std::set<std::string, std::less<>> all{"savanna", "savanna_plateau", "windswept_savanna"};
    return all;
}

const std::set<std::string, std::less<>>& jungleBiomes() {
    static const std::set<std::string, std::less<>> all{"bamboo_jungle", "jungle", "sparse_jungle"};
    return all;
}

const std::set<std::string, std::less<>>& badlandsBiomes() {
    static const std::set<std::string, std::less<>> all{"badlands", "eroded_badlands", "wooded_badlands"};
    return all;
}

} // namespace mobSpawnVariantData

} // namespace basalt::server::runtime::world
