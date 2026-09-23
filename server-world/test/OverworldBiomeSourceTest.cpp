#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

struct Run {
    std::size_t distinct = 0;
    std::int64_t checksum = 0;
    std::string counts;
};

[[nodiscard]] Run run(const OverworldBiomeSource& source, bool zoomed) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::map<std::string, std::int32_t> counts;
    for (std::int32_t index = 0; index < 4096; ++index) {
        const std::int32_t x = floorMod(index * 37, 2048) - 1024;
        const std::int32_t z = floorMod(index * 91, 2048) - 1024;
        const std::int32_t y = floorMod(index * 13, 256) - 64;
        const std::string_view path = zoomed ? source.blockBiomeAt(x, y, z) : source.biomeAt(x, y, z);
        ++counts[std::string{path}];
        for (const char character : path) {
            checksum =
                (checksum ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
        }
    }
    Run result;
    result.distinct = counts.size();
    result.checksum = static_cast<std::int64_t>(checksum);
    for (const auto& [path, count] : counts) {
        if (!result.counts.empty()) {
            result.counts += ",";
        }
        result.counts += path + "=" + std::to_string(count);
    }
    return result;
}

} // namespace

TEST_CASE("the overworld biome source walks the climate tree to the same biomes") {
    const NoiseRouter router = testsupport::syntheticRouter(0, "0.0", {.fields = testsupport::climateFields()});

    SUBCASE("direct") {
        const OverworldBiomeSource source{router, std::nullopt};
        const Run result = run(source, false);
        CHECK(result.distinct == 55);
        CHECK(result.checksum == 4462209183397504177LL);
        CHECK(result.counts ==
              "badlands=8,bamboo_jungle=46,beach=104,birch_forest=64,cherry_grove=18,cold_ocean=152,dappled_forest="
              "72,dark_forest=142,deep_cold_ocean=96,deep_dark=340,deep_frozen_ocean=26,deep_lukewarm_ocean=42,deep"
              "_ocean=152,desert=20,dripstone_caves=116,eroded_badlands=6,flower_forest=98,forest=312,frozen_ocean="
              "96,frozen_peaks=12,frozen_river=26,grove=20,ice_spikes=28,jungle=80,lukewarm_ocean=126,lush_caves=16"
              "8,mangrove_swamp=28,meadow=22,mushroom_fields=12,ocean=282,old_growth_birch_forest=52,old_growth_pin"
              "e_taiga=30,old_growth_spruce_taiga=24,pale_garden=12,plains=364,river=212,savanna=144,savanna_platea"
              "u=16,snowy_beach=14,snowy_plains=72,snowy_slopes=10,snowy_taiga=48,sparse_jungle=26,stony_peaks=4,st"
              "ony_shore=58,sulfur_caves=10,sunflower_plains=108,swamp=44,taiga=80,warm_ocean=18,windswept_forest=4"
              ",windswept_gravelly_hills=2,windswept_hills=4,windswept_savanna=4,wooded_badlands=22");
    }

    SUBCASE("zoomed") {
        const OverworldBiomeSource source{router, std::optional<std::int64_t>{1729LL}};
        const Run result = run(source, true);
        CHECK(result.distinct == 55);
        CHECK(result.checksum == 2643869539689583349LL);
        CHECK(result.counts ==
              "badlands=8,bamboo_jungle=44,beach=100,birch_forest=58,cherry_grove=18,cold_ocean=154,dappled_forest="
              "74,dark_forest=144,deep_cold_ocean=100,deep_dark=342,deep_frozen_ocean=26,deep_lukewarm_ocean=42,dee"
              "p_ocean=146,desert=14,dripstone_caves=116,eroded_badlands=6,flower_forest=102,forest=312,frozen_ocea"
              "n=94,frozen_peaks=10,frozen_river=24,grove=26,ice_spikes=28,jungle=86,lukewarm_ocean=134,lush_caves="
              "168,mangrove_swamp=28,meadow=24,mushroom_fields=12,ocean=282,old_growth_birch_forest=50,old_growth_p"
              "ine_taiga=26,old_growth_spruce_taiga=22,pale_garden=16,plains=352,river=218,savanna=142,savanna_plat"
              "eau=16,snowy_beach=14,snowy_plains=76,snowy_slopes=10,snowy_taiga=48,sparse_jungle=28,stony_peaks=4,"
              "stony_shore=56,sulfur_caves=10,sunflower_plains=108,swamp=44,taiga=74,warm_ocean=18,windswept_forest"
              "=8,windswept_gravelly_hills=2,windswept_hills=6,windswept_savanna=6,wooded_badlands=20");
    }
}
