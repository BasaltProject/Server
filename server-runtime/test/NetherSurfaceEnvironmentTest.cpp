#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/NetherSurfaceEnvironment.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <doctest.h>

#include <array>
#include <map>
#include <string>

using basalt::server::random::PositionalRandomFactory;
using basalt::server::random::RandomSource;
using basalt::server::runtime::world::NetherSurfaceEnvironment;
using basalt::server::runtime::world::RealNoiseParameters;
using basalt::server::world::noise::NormalNoise;
using basalt::server::world::terrain::NetherBiomeSource;
using basalt::server::world::terrain::NetherNoiseRouter;
namespace df = basalt::server::world::densityfunction::functions;

namespace {

/// A Nether router whose floor is solid up to the height asked for.
[[nodiscard]] NetherNoiseRouter router(basalt::server::world::densityfunction::Function density) {
    return NetherNoiseRouter{.temperature = df::zero(), .vegetation = df::zero(), .finalDensity = std::move(density)};
}

struct FakeJar {
    std::map<std::string, std::string> files;
    std::map<std::string, std::int32_t> reads;

    [[nodiscard]] RealNoiseParameters::ReadText readText() {
        return [this](std::string_view name) -> std::optional<std::string> {
            ++reads[std::string{name}];
            const auto found = files.find(std::string{name});
            return found == files.end() ? std::nullopt : std::optional{found->second};
        };
    }
};

[[nodiscard]] RealNoiseParameters::ParseParameters parser() {
    return [](std::string_view json) -> std::optional<NormalNoise::Parameters> {
        return NormalNoise::Parameters{.firstOctave = std::stoi(std::string{json}), .amplitudes = {1.0}};
    };
}

[[nodiscard]] std::string noiseEntry(const std::string& name) {
    return "data/minecraft/worldgen/noise/" + name + ".json";
}

} // namespace

TEST_CASE("the surface scan starts at the Nether's own roof and stops at its floor") {
    FakeJar jar;
    const NetherBiomeSource biomes{df::zero(), df::zero()};

    // Solid up to y 40 and air above it.
    const NetherNoiseRouter shallow = router(df::yClampedGradient(40, 41, 1.0, -1.0));
    const NetherSurfaceEnvironment low{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2),
                                       shallow,        biomes,   0};
    CHECK(low.estimatedSurfaceHeight(8, 8) == 41);

    // parity: 128 blocks of Nether, so anything solid above 127 is out of reach of the scan.
    const NetherNoiseRouter high = router(df::yClampedGradient(200, 201, 1.0, -1.0));
    const NetherSurfaceEnvironment roofed{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2),
                                          high,           biomes,   0};
    CHECK(roofed.estimatedSurfaceHeight(8, 8) == 128);

    // And a column with nothing solid in it answers with the floor, not with a negative height.
    const NetherNoiseRouter empty = router(df::zero());
    const NetherSurfaceEnvironment hollow{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2),
                                          empty,          biomes,   0};
    CHECK(hollow.estimatedSurfaceHeight(8, 8) == 0);
}

TEST_CASE("zero density is nothing to stand on, the same as air") {
    FakeJar jar;
    const NetherBiomeSource biomes{df::zero(), df::zero()};
    const NetherNoiseRouter built = router(df::yClampedGradient(40, 42, 1.0, -1.0));
    const NetherSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2),
                                               built,          biomes,   0};

    // The gradient is exactly zero at y 41, which the scan walks past.
    CHECK(environment.estimatedSurfaceHeight(8, 8) == 41);
}

TEST_CASE("the seed for the biome zoom comes back as it was handed in, and nothing is steep") {
    FakeJar jar;
    const NetherBiomeSource biomes{df::zero(), df::zero()};
    const NetherNoiseRouter built = router(df::zero());
    const NetherSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2),
                                               built,          biomes,   987654321};

    CHECK(environment.biomeZoomSeed() == 987654321);
    CHECK_FALSE(environment.isSteep(0, 0));
    CHECK_FALSE(environment.isSteep(1000, -1000));
}

TEST_CASE("each named noise is the one the jar shipped under that name") {
    FakeJar jar{.files = {{noiseEntry("surface"), "-6"}, {noiseEntry("surface_secondary"), "-3"}}};
    const NetherBiomeSource biomes{df::zero(), df::zero()};
    const NetherNoiseRouter built = router(df::zero());
    const NetherSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(7, 9),
                                               built,          biomes,   0};

    const double first = environment.sample2d("surface", 100, 200);
    CHECK(first != doctest::Approx(environment.surfaceSecondaryNoise(100, 200)));
    CHECK(environment.sample2d("surface", 300, 400) != doctest::Approx(first));
    CHECK(jar.reads[noiseEntry("surface")] == 1);
    // The third dimension is read, so the same column at two heights is not the same sample.
    CHECK(environment.sample3d("surface", 100, 0, 200) == doctest::Approx(first));
    CHECK(environment.sample3d("surface", 100, 40, 200) != doctest::Approx(first));
    // A noise the jar does not ship reads as flat zero rather than failing.
    CHECK(environment.sample2d("nothing_of_the_sort", 8, 8) == doctest::Approx(0.0));
}

TEST_CASE("the depth jitter is drawn at the column itself, at height zero") {
    FakeJar jar{.files = {{noiseEntry("surface"), "-6"}}};
    const NetherBiomeSource biomes{df::zero(), df::zero()};
    const NetherNoiseRouter built = router(df::zero());
    const PositionalRandomFactory factory = PositionalRandomFactory::xoroshiro(5, 6);
    const NetherSurfaceEnvironment environment{jar.readText(), parser(), factory, built, biomes, 0};

    // The jitter is worth a quarter of a block, so it only shows where the rest of the sum is
    // already within a quarter of the next whole one. Find such a column and pin it there.
    bool found = false;
    for (std::int32_t x = 0; x < 4000 && !found; ++x) {
        const double base = (environment.sample2d("surface", x, 0) * 2.75) + 3.0;
        const auto depthWith = [base](double jitter) { return static_cast<std::int32_t>(base + (jitter * 0.25)); };
        const std::int32_t atZero = depthWith(factory.at(x, 0, 0).nextDouble());
        if (atZero == depthWith(factory.at(x, 1, 0).nextDouble())) {
            continue;
        }
        found = true;
        CHECK(environment.surfaceDepthNoise(x, 0) == atZero);
    }
    REQUIRE(found);
}

TEST_CASE("the vertical gradient random is one factory per name and depends on the position") {
    FakeJar jar;
    const NetherBiomeSource biomes{df::zero(), df::zero()};
    const NetherNoiseRouter built = router(df::zero());
    const NetherSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(3, 4),
                                               built,          biomes,   0};

    const float roof = environment.verticalGradientRandom("bedrock_roof", 8, 120, 8);
    CHECK(roof >= 0.0F);
    CHECK(roof < 1.0F);
    CHECK(environment.verticalGradientRandom("bedrock_roof", 8, 120, 8) == doctest::Approx(roof));
    CHECK(environment.verticalGradientRandom("bedrock_roof", 8, 121, 8) != doctest::Approx(roof));
    CHECK(environment.verticalGradientRandom("bedrock_floor", 8, 120, 8) != doctest::Approx(roof));
}

TEST_CASE("the positional random is the world's own, at the block asked for") {
    FakeJar jar;
    const NetherBiomeSource biomes{df::zero(), df::zero()};
    const NetherNoiseRouter built = router(df::zero());
    const PositionalRandomFactory factory = PositionalRandomFactory::xoroshiro(11, 13);
    const NetherSurfaceEnvironment environment{jar.readText(), parser(), factory, built, biomes, 0};

    RandomSource expected = factory.at(4, 5, 6);
    RandomSource actual = environment.positionalRandom(4, 5, 6);
    CHECK(actual.nextInt() == expected.nextInt());
    CHECK(actual.nextLong() == expected.nextLong());
}

TEST_CASE("a biome name is handed out by reference and the same one every time") {
    FakeJar jar;
    const NetherBiomeSource biomes{df::zero(), df::zero()};
    const NetherNoiseRouter built = router(df::zero());
    const NetherSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2),
                                               built,          biomes,   0};

    const std::string& first = environment.biomeAt(8, 64, 8);
    CHECK_FALSE(first.empty());
    CHECK(&environment.biomeAt(8, 64, 8) == &first);
    CHECK(&environment.noiseBiomeAt(2, 16, 2) == &environment.noiseBiomeAt(2, 16, 2));
    CHECK(environment.biomeAt(8, 64, 8, nullptr) == first);
}
