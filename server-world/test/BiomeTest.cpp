#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/server/world/terrain/BiomeZoomer.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;

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
