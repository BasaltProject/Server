#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/OverworldAquifer.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using basalt::server::registry::BlockState;
using namespace basalt::server::world::terrain;
namespace fn = basalt::server::world::densityfunction::functions;

namespace {

/// Derived from the name exactly as the router test does, so both describe the same world.
std::shared_ptr<const noise::NormalNoise> syntheticNoise(std::string_view id) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : id) {
        hash = (hash ^ static_cast<std::uint64_t>(static_cast<unsigned char>(character))) * 1099511628211ULL;
    }
    basalt::server::random::RandomSource source =
        basalt::server::random::RandomSource::xoroshiro(static_cast<std::int64_t>(hash));
    const std::array<double, 3> amplitudes = {1.0, 0.6, 0.3};
    return std::make_shared<const noise::NormalNoise>(
        noise::NormalNoise::create(source, -(static_cast<std::int32_t>(id.size()) % 5 + 1), amplitudes));
}

} // namespace

TEST_CASE("the aquifer picks the same substances as the frozen kotlin") {
    const NoiseLookup lookup = syntheticNoise;
    const OverworldNoiseRouter router =
        noiseRouterData::overworld(lookup, fn::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0));

    // A sweep of near-zero densities rather than the router's own, whose extremes never reach the
    // barrier and flow paths this class exists for.
    constexpr std::array<double, 5> kDensities = {-0.001, -0.05, -0.2, -1.0, 0.5};
    std::string names;
    std::string flags;
    for (std::int32_t chunk = 0; chunk < 2; ++chunk) {
        OverworldAquifer aquifer{router, 0x123456789LL, chunk, -2, -64, 384};
        for (std::int32_t i = 0; i < 64; ++i) {
            const std::int32_t x = chunk * 16 + (i * 7) % 16;
            const std::int32_t z = -32 + (i * 11) % 16;
            const std::int32_t y = -64 + i * 6;
            const BlockState* substance =
                aquifer.computeSubstance(x, y, z, kDensities[static_cast<std::size_t>(i) % kDensities.size()]);
            names.append(substance == nullptr ? "stone" : substance->block.path());
            names.push_back(',');
            flags.push_back(aquifer.shouldScheduleFluidUpdate() ? '1' : '0');
        }
    }

    CHECK(names ==
          "lava,lava,air,air,stone,air,air,air,air,stone,air,air,air,air,stone,air,stone,water,water,stone,water,water,"
          "air,air,stone,air,air,air,air,stone,air,air,air,air,stone,air,air,air,air,stone,air,air,air,air,stone,air,"
          "air,air,air,stone,air,air,air,air,stone,air,air,air,air,stone,air,air,air,air,lava,lava,air,air,stone,air,"
          "air,air,air,stone,air,air,stone,air,stone,air,air,water,water,stone,water,water,air,air,stone,air,air,air,"
          "air,stone,air,air,air,air,stone,air,air,air,air,stone,air,air,air,air,stone,air,air,air,air,stone,air,air,"
          "air,air,stone,air,air,air,air,stone,air,air,air,air,");
    CHECK(flags == "001000000000010000000000000000000000000000000000000000000000000000000000000000001100000000000000000"
                   "00000000000000000000000000000");
}
