#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/NoiseColumnHeights.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/OverworldOreVeinifier.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// The same synthetic parameters the router oracle uses, so what this pins is the two algorithms and
/// not Mojang's numbers.
std::shared_ptr<const noise::NormalNoise> syntheticNoise(std::string_view id) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : id) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(character))) * 1099511628211ULL;
    }
    rng::RandomSource source = rng::RandomSource::xoroshiro(static_cast<std::int64_t>(hash));
    const std::array<double, 3> amplitudes{1.0, 0.6, 0.3};
    return std::make_shared<const noise::NormalNoise>(
        noise::NormalNoise::create(source, -(static_cast<std::int32_t>(id.size()) % 5 + 1), amplitudes));
}

[[nodiscard]] OverworldNoiseRouter buildRouter() {
    return noiseRouterData::overworld(syntheticNoise,
                                      densityfunction::functions::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0));
}

} // namespace

TEST_CASE("the ore veins replace what the frozen kotlin replaces") {
    const OverworldNoiseRouter router = buildRouter();
    const OverworldOreVeinifier veinifier{router, 5417LL};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t replaced = 0;
    std::map<std::string, std::int32_t> counts;
    // A dense block rather than a strided walk: the positions that pass the veininess threshold are
    // clustered, and only inside a cluster does the ore branch ever beat the filler.
    for (std::int32_t x = -112; x <= 112; ++x) {
        for (std::int32_t z = -112; z <= 112; ++z) {
            for (const std::int32_t y : {-40, -30, -20, -12, 6, 14, 22, 30, 44}) {
                const std::optional<std::int32_t> state = veinifier.compute(x, y, z);
                const std::int32_t id = state.value_or(-1);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (!state.has_value()) {
                    continue;
                }
                ++replaced;
                ++counts[registry::blockStateDefinitions::blockOf(*state)->path()];
            }
        }
    }
    std::string summary;
    for (const auto& [path, count] : counts) {
        if (!summary.empty()) {
            summary += ",";
        }
        summary += path + "=" + std::to_string(count);
    }
    CHECK(replaced == 625);
    CHECK(summary == "copper_ore=64,deepslate_iron_ore=47,granite=294,raw_copper_block=2,tuff=218");
    CHECK(static_cast<std::int64_t>(checksum) == 1242193994256256609LL);
}

TEST_CASE("the column heights match the frozen kotlin") {
    const OverworldNoiseRouter router = buildRouter();
    const NoiseColumnHeights heights{router};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int64_t surfaceSum = 0;
    std::int64_t floorSum = 0;
    std::int32_t solid = 0;
    for (std::int32_t index = 0; index < 128; ++index) {
        const std::int32_t x = (index * 53) % 384 - 192;
        const std::int32_t z = (index * 29) % 384 - 192;
        const std::int32_t surface = heights.surfaceHeight(x, z);
        const std::int32_t floor = heights.oceanFloorHeight(x, z);
        surfaceSum += surface;
        floorSum += floor;
        if (heights.isSolid(x, surface - 1, z)) {
            ++solid;
        }
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(surface))) * 1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(floor))) * 1099511628211ULL;
    }
    CHECK(surfaceSum == 8586);
    CHECK(floorSum == 7826);
    CHECK(solid == 52);
    CHECK(static_cast<std::int64_t>(checksum) == -8966332820666116839LL);
}
