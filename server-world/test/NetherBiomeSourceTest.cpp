#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/SyntheticRouter.hpp"

#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using basalt::server::world::terrain::NetherBiomeSource;
using basalt::server::world::testsupport::syntheticRouter;

namespace {

[[nodiscard]] terrain::NoiseRouter climateRouter(const std::string& temperature, const std::string& vegetation) {
    return syntheticRouter(0, "0.0",
                           {.minY = 0,
                            .height = 128,
                            .defaultBlock = "minecraft:netherrack",
                            .fields = {{"temperature", temperature}, {"vegetation", vegetation}}});
}

[[nodiscard]] std::string gradient(std::int32_t from, std::int32_t to, double fromValue, double toValue) {
    return R"({"type": "minecraft:gradient", "axis": "y", "from_coordinate": )" + std::to_string(from) +
           R"(, "to_coordinate": )" + std::to_string(to) + R"(, "from_value": )" + std::to_string(fromValue) +
           R"(, "to_value": )" + std::to_string(toValue) + "}";
}

/// The biome's own initial, which is enough to tell the five Nether biomes apart.
[[nodiscard]] char initial(std::string_view biome) {
    const std::size_t colon = biome.find(':');
    return biome[colon == std::string_view::npos ? 0 : colon + 1];
}

} // namespace

TEST_CASE("the nether biome source resolves what the parameter list resolves") {
    // Sweeping the two axes the Nether actually uses walks the whole parameter tree, including the
    // offsets that bias warped forest and basalt deltas.
    std::string grid;
    for (std::int32_t temperatureStep = -10; temperatureStep <= 10; ++temperatureStep) {
        for (std::int32_t vegetationStep = -10; vegetationStep <= 10; ++vegetationStep) {
            const terrain::NoiseRouter router =
                climateRouter(std::to_string(temperatureStep / 10.0), std::to_string(vegetationStep / 10.0));
            const NetherBiomeSource source{router};
            grid += initial(source.biomeAt(0, 64, 0));
        }
        grid += "|";
    }
    CHECK(
        grid ==
        "sbbbbbbbbbbbbbbbbbbbb|ssbbbbbbbbbbbbbbbbbbb|sssbbbbbbbbbbbbbbbbbw|ssssbbbbbbbbbbbbbbbww|sssssbbbbbbbbbbbbbwww|"
        "ssssssbbbbbbbbbbbwwww|sssssssbbbbbbbbbwwwww|ssssssssbbbbbbbwwwwww|ssssssssnnnnnnwwwwwww|ssssssssnnnnnnwwwwwww|"
        "ssssssssnnnnnnwwwwwww|ssssssssnnnnnnwwwwwww|ssssssssnnnnnnwwwwwww|sssssssccccccccwwwwww|ssssssccccccccccwwwww|"
        "sssssscccccccccccwwww|ssssscccccccccccccwww|ssssccccccccccccccwww|sssccccccccccccccccww|ssccccccccccccccccccw|"
        "ssccccccccccccccccccc|");
}

TEST_CASE("the zoomed nether lookup answers from the quart the zoom picks") {
    // A y gradient makes the answer depend on which quart the Voronoi lands on, which a constant
    // noise would hide. Which quart it picks is pinned by the zoomer's own test.
    const terrain::NoiseRouter router = climateRouter(gradient(0, 128, -1.0, 1.0), gradient(0, 40, 1.0, -1.0));
    const NetherBiomeSource zoomed{router, 987654321LL};
    std::string walk;
    for (std::int32_t index = 0; index < 96; ++index) {
        walk += initial(zoomed.blockBiomeAt(index * 37 - 900, 20 + index / 4, index * -53 + 400));
    }
    CHECK(walk == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbssssssssssssssssssssssssssssssssssssssssssssssss");
}
