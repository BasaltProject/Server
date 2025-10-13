#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OverworldBiomeSource.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace rng = basalt::server::random;
namespace fn = basalt::server::world::densityfunction::functions;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// The same synthetic parameters the router oracle uses, so what this pins is the sampling and the
/// climate search, not Mojang's numbers.
std::shared_ptr<const noise::NormalNoise> syntheticNoise(std::string_view id) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : id) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    rng::RandomSource source = rng::RandomSource::xoroshiro(static_cast<std::int64_t>(hash));
    const std::array<double, 3> amplitudes{1.0, 0.6, 0.3};
    return std::make_shared<const noise::NormalNoise>(
        noise::NormalNoise::create(source, -(static_cast<std::int32_t>(id.size()) % 5 + 1), amplitudes));
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

TEST_CASE("the overworld biome source picks what the frozen kotlin picks") {
    const OverworldNoiseRouter router =
        noiseRouterData::overworld(syntheticNoise, fn::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0));

    SUBCASE("direct") {
        const OverworldBiomeSource source{router, std::nullopt};
        const Run result = run(source, false);
        CHECK(result.distinct == 51);
        CHECK(result.checksum == -8978417841032397371LL);
        CHECK(result.counts ==
              "badlands=4,bamboo_jungle=24,beach=136,birch_forest=96,cherry_grove=16,cold_ocean=208,dark_forest=100,"
              "deep_cold_ocean=72,deep_dark=44,deep_frozen_ocean=22,deep_lukewarm_ocean=68,deep_ocean=118,desert=34,"
              "dripstone_caves=78,eroded_badlands=2,flower_forest=74,forest=424,frozen_ocean=34,frozen_peaks=6,"
              "frozen_river=32,grove=12,ice_spikes=4,jungle=68,lukewarm_ocean=194,lush_caves=168,mangrove_swamp=14,"
              "meadow=38,ocean=392,old_growth_birch_forest=94,old_growth_pine_taiga=28,old_growth_spruce_taiga=32,"
              "pale_garden=2,plains=444,river=344,savanna=174,savanna_plateau=8,snowy_beach=20,snowy_plains=42,"
              "snowy_slopes=2,snowy_taiga=34,sparse_jungle=54,stony_shore=44,sulfur_caves=18,sunflower_plains=74,"
              "swamp=40,taiga=124,warm_ocean=20,windswept_forest=4,windswept_gravelly_hills=2,windswept_savanna=6,"
              "wooded_badlands=4");
    }

    SUBCASE("zoomed") {
        const OverworldBiomeSource source{router, std::optional<std::int64_t>{1729LL}};
        const Run result = run(source, true);
        CHECK(result.distinct == 51);
        CHECK(result.checksum == 6174079075834419721LL);
        CHECK(result.counts ==
              "badlands=4,bamboo_jungle=20,beach=124,birch_forest=84,cherry_grove=16,cold_ocean=200,dark_forest=104,"
              "deep_cold_ocean=60,deep_dark=44,deep_frozen_ocean=18,deep_lukewarm_ocean=64,deep_ocean=130,desert=28,"
              "dripstone_caves=82,flower_forest=80,forest=434,frozen_ocean=38,frozen_peaks=4,frozen_river=26,grove=10,"
              "ice_spikes=8,jungle=64,lukewarm_ocean=192,lush_caves=184,mangrove_swamp=8,meadow=32,ocean=398,"
              "old_growth_birch_forest=84,old_growth_pine_taiga=24,old_growth_spruce_taiga=34,plains=444,river=324,"
              "savanna=222,savanna_plateau=10,snowy_beach=12,snowy_plains=58,snowy_slopes=4,snowy_taiga=50,"
              "sparse_jungle=50,stony_peaks=2,stony_shore=40,sulfur_caves=12,sunflower_plains=74,swamp=42,taiga=110,"
              "warm_ocean=30,windswept_forest=4,windswept_gravelly_hills=2,windswept_hills=2,windswept_savanna=4,"
              "wooded_badlands=2");
    }
}
