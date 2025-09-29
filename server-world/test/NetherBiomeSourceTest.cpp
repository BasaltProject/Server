#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using basalt::server::world::terrain::NetherBiomeSource;
namespace functions = basalt::server::world::densityfunction::functions;

namespace {

/// The biome's own initial, which is enough to tell the five Nether biomes apart.
[[nodiscard]] char initial(std::string_view biome) {
    const std::size_t colon = biome.find(':');
    return biome[colon == std::string_view::npos ? 0 : colon + 1];
}

} // namespace

TEST_CASE("the nether biome source resolves what the frozen kotlin resolves") {
    // Sweeping the two axes the Nether actually uses walks the whole parameter tree, including the
    // offsets that bias warped forest and basalt deltas.
    std::string grid;
    for (std::int32_t temperatureStep = -10; temperatureStep <= 10; ++temperatureStep) {
        for (std::int32_t vegetationStep = -10; vegetationStep <= 10; ++vegetationStep) {
            const NetherBiomeSource source{functions::constant(temperatureStep / 10.0),
                                           functions::constant(vegetationStep / 10.0)};
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

TEST_CASE("the zoomed nether lookup answers what the frozen kotlin answers") {
    // A y gradient makes the answer depend on which quart the Voronoi lands on, which a constant
    // noise would hide. Which quart it picks is pinned by the zoomer's own test.
    const NetherBiomeSource zoomed{functions::yClampedGradient(0, 128, -1.0, 1.0),
                                   functions::yClampedGradient(0, 40, 1.0, -1.0), 987654321LL};
    std::string walk;
    for (std::int32_t index = 0; index < 96; ++index) {
        walk += initial(zoomed.blockBiomeAt(index * 37 - 900, 20 + index / 4, index * -53 + 400));
    }
    CHECK(walk == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbssssssssssssssssssssssssssssssssssssssssssssssss");
}
