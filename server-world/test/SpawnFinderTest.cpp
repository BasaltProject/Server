#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/climate/SpawnFinder.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::climate;
namespace rng = basalt::server::random;
namespace fn = basalt::server::world::densityfunction::functions;

namespace {

/// The same synthetic parameters the other terrain oracles use, so what this pins is the spiral and
/// the fitness, not Mojang's numbers.
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

} // namespace

TEST_CASE("the spawn search lands where the frozen kotlin lands") {
    const terrain::OverworldNoiseRouter router =
        terrain::noiseRouterData::overworld(syntheticNoise, fn::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0));
    const Sampler sampler{router.temperature, router.vegetation, router.continents,
                          router.erosion,     router.depth,      router.ridges};

    SUBCASE("overworld") {
        const auto [x, z] = spawnFinder::findSpawnPosition(sampler);
        CHECK(x == -25);
        CHECK(z == -68);
    }

    SUBCASE("single") {
        // One target only: the second weirdness band is what the two-entry list adds.
        const std::vector<ParameterPoint> single{spawnFinder::overworldTarget().front()};
        const auto [x, z] = spawnFinder::findSpawnPosition(sampler, single);
        CHECK(x == -9);
        CHECK(z == -2);
    }

    SUBCASE("empty") {
        const auto [x, z] = spawnFinder::findSpawnPosition(sampler, {});
        CHECK(x == 0);
        CHECK(z == 0);
    }
}
