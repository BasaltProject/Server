#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/PerlinSimplexNoise.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>

#include <doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <string>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;

namespace {

[[nodiscard]] std::int64_t bits(double value) { return std::bit_cast<std::int64_t>(value); }

} // namespace

TEST_CASE("the fixed-seed simplex octaves match the frozen kotlin bit for bit") {
    basalt::server::random::RandomSource singleSource = basalt::server::random::RandomSource::legacy(1234LL);
    basalt::server::random::RandomSource threeSource = basalt::server::random::RandomSource::legacy(3456LL);
    const std::array<std::int32_t, 1> singleOctaves = {0};
    const std::array<std::int32_t, 3> threeOctaves = {-2, -1, 0};
    const noise::PerlinSimplexNoise single{singleSource, singleOctaves};
    const noise::PerlinSimplexNoise threeOctave{threeSource, threeOctaves};

    constexpr std::array<std::int64_t, 8> kSingle = {-4624139141725278784, 4603535281521814056, 4604240672714077158,
                                                     4603565438189204023,  4599123578272459314, -4619668459407831984,
                                                     4595609573764067542,  4604212320260558994};
    constexpr std::array<std::int64_t, 8> kThree = {-4628448138212556216, 4584843535077773730, 4602328100120860976,
                                                    4596457774124310389,  4584599227883672056, 4600827102736893586,
                                                    -4629499750512591955, -4623026963361167786};
    constexpr std::array<std::int64_t, 8> kThreeStart = {
        4595808130894037030,  -4621688538847035373, 4603128423517541530,  -4627156795395066203,
        -4619802531451546840, -4625171094311740814, -4630988228395787448, 4598319374718461240};

    for (std::size_t i = 0; i < kSingle.size(); ++i) {
        CAPTURE(i);
        const double x = static_cast<double>(i) * 13.5 - 40.0;
        const double y = static_cast<double>(i) * -7.25 + 11.0;
        CHECK(bits(single.getValue(x, y)) == kSingle[i]);
        CHECK(bits(threeOctave.getValue(x, y)) == kThree[i]);
        CHECK(bits(threeOctave.getValue(x, y, true)) == kThreeStart[i]);
    }
}

TEST_CASE("the voronoi zoom picks the same quart cell as the frozen kotlin") {
    const BiomeZoomer zoomer{0x123456789ABCLL};
    std::string picked;
    for (std::int32_t i = 0; i < 24; ++i) {
        zoomer.biomeAt(i * 37 - 400, i * 11 - 60, i * -53 + 300,
                       [&picked](std::int32_t qx, std::int32_t qy, std::int32_t qz) {
                           picked.append(std::to_string(qx)).push_back('/');
                           picked.append(std::to_string(qy)).push_back('/');
                           picked.append(std::to_string(qz)).push_back(',');
                       });
    }
    CHECK(picked == "-101/-15/75,-91/-13/61,-82/-10/48,-73/-7/35,-63/-4/21,-54/-2/8,-45/1/-5,-36/4/-18,-26/7/-32,-17/9/"
                    "-44,-8/12/-58,1/15/-71,10/17/-84,20/21/-98,29/23/-111,38/26/-125,48/29/-138,57/31/-151,66/34/"
                    "-164,75/37/-177,84/39/-190,93/42/-204,103/45/-217,112/47/-231,");
}

TEST_CASE("biome temperature agrees with the frozen kotlin on snow and melting") {
    std::string flags;
    for (const std::string_view biome :
         {"minecraft:plains", "minecraft:frozen_ocean", "minecraft:desert", "minecraft:unknown"}) {
        for (std::int32_t i = 0; i < 6; ++i) {
            const std::int32_t x = i * 91 - 200;
            const std::int32_t y = 40 + i * 30;
            const std::int32_t z = i * -67 + 150;
            flags.push_back(biomeTemperature::coldEnoughToSnow(biome, x, y, z) ? '1' : '0');
            flags.push_back(biomeTemperature::shouldMeltFrozenOceanIcebergSlightly(biome, x, y, z) ? '1' : '0');
        }
    }
    CHECK(flags == "010101010101010101101010010101010101010101010101");
}
