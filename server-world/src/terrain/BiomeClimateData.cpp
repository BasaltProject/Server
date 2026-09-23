#include <basalt/server/world/terrain/BiomeClimateData.hpp>

#include <map>

namespace basalt::server::world::terrain {
namespace {

const std::map<std::string_view, BiomeClimate>& table() {
    static const std::map<std::string_view, BiomeClimate> entries = {
        {"plains", BiomeClimate{0.8F, false, true}},
        {"sunflower_plains", BiomeClimate{0.8F, false, true}},
        {"snowy_plains", BiomeClimate{0.0F, false, true}},
        {"ice_spikes", BiomeClimate{0.0F, false, true}},
        {"desert", BiomeClimate{2.0F, false, false}},
        {"swamp", BiomeClimate{0.8F, false, true}},
        {"mangrove_swamp", BiomeClimate{0.8F, false, true}},
        {"forest", BiomeClimate{0.7F, false, true}},
        {"flower_forest", BiomeClimate{0.7F, false, true}},
        {"birch_forest", BiomeClimate{0.6F, false, true}},
        {"dark_forest", BiomeClimate{0.7F, false, true}},
        {"dappled_forest", BiomeClimate{0.6F, false, true}},
        {"pale_garden", BiomeClimate{0.7F, false, true}},
        {"old_growth_birch_forest", BiomeClimate{0.6F, false, true}},
        {"old_growth_pine_taiga", BiomeClimate{0.3F, false, true}},
        {"old_growth_spruce_taiga", BiomeClimate{0.25F, false, true}},
        {"taiga", BiomeClimate{0.25F, false, true}},
        {"snowy_taiga", BiomeClimate{-0.5F, false, true}},
        {"savanna", BiomeClimate{2.0F, false, false}},
        {"savanna_plateau", BiomeClimate{2.0F, false, false}},
        {"windswept_hills", BiomeClimate{0.2F, false, true}},
        {"windswept_gravelly_hills", BiomeClimate{0.2F, false, true}},
        {"windswept_forest", BiomeClimate{0.2F, false, true}},
        {"windswept_savanna", BiomeClimate{2.0F, false, false}},
        {"jungle", BiomeClimate{0.95F, false, true}},
        {"sparse_jungle", BiomeClimate{0.95F, false, true}},
        {"bamboo_jungle", BiomeClimate{0.95F, false, true}},
        {"badlands", BiomeClimate{2.0F, false, false}},
        {"eroded_badlands", BiomeClimate{2.0F, false, false}},
        {"wooded_badlands", BiomeClimate{2.0F, false, false}},
        {"meadow", BiomeClimate{0.5F, false, true}},
        {"cherry_grove", BiomeClimate{0.5F, false, true}},
        {"grove", BiomeClimate{-0.2F, false, true}},
        {"snowy_slopes", BiomeClimate{-0.3F, false, true}},
        {"frozen_peaks", BiomeClimate{-0.7F, false, true}},
        {"jagged_peaks", BiomeClimate{-0.7F, false, true}},
        {"stony_peaks", BiomeClimate{1.0F, false, true}},
        {"river", BiomeClimate{0.5F, false, true}},
        {"frozen_river", BiomeClimate{0.0F, false, true}},
        {"beach", BiomeClimate{0.8F, false, true}},
        {"snowy_beach", BiomeClimate{0.05F, false, true}},
        {"stony_shore", BiomeClimate{0.2F, false, true}},
        {"warm_ocean", BiomeClimate{0.5F, false, true}},
        {"lukewarm_ocean", BiomeClimate{0.5F, false, true}},
        {"deep_lukewarm_ocean", BiomeClimate{0.5F, false, true}},
        {"ocean", BiomeClimate{0.5F, false, true}},
        {"deep_ocean", BiomeClimate{0.5F, false, true}},
        {"cold_ocean", BiomeClimate{0.5F, false, true}},
        {"deep_cold_ocean", BiomeClimate{0.5F, false, true}},
        {"frozen_ocean", BiomeClimate{0.0F, true, true}},
        {"deep_frozen_ocean", BiomeClimate{0.5F, true, true}},
        {"mushroom_fields", BiomeClimate{0.9F, false, true}},
        {"dripstone_caves", BiomeClimate{0.8F, false, true}},
        {"lush_caves", BiomeClimate{0.5F, false, true}},
        {"deep_dark", BiomeClimate{0.8F, false, true}},
        {"sulfur_caves", BiomeClimate{0.8F, false, true}},
    };
    return entries;
}

} // namespace

BiomeClimate biomeClimateFor(std::string_view id) {
    const std::size_t colon = id.find(':');
    const std::string_view path = colon == std::string_view::npos ? id : id.substr(colon + 1);
    const auto found = table().find(path);
    return found == table().end() ? BiomeClimate{} : found->second;
}

} // namespace basalt::server::world::terrain
