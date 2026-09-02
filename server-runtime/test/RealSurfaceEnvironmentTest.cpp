#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealSurfaceEnvironment.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <doctest.h>

#include <array>
#include <map>
#include <string>

using basalt::server::random::PositionalRandomFactory;
using basalt::server::random::RandomSource;
using basalt::server::runtime::world::RealNoiseParameters;
using basalt::server::runtime::world::RealSurfaceEnvironment;
using basalt::server::world::noise::NormalNoise;
using basalt::server::world::terrain::OverworldNoiseRouter;
namespace df = basalt::server::world::densityfunction::functions;

namespace {

/// Answers with the coordinate it was asked about, so which block the router saw is exact.
class CoordinateProbe final : public basalt::server::world::densityfunction::DensityFunction {
public:
    enum class Axis : std::uint8_t { X, Y, Z };

    explicit CoordinateProbe(Axis axis) : axis_(axis) {}

    [[nodiscard]] double compute(basalt::server::world::densityfunction::FunctionContext& context) const override {
        switch (axis_) {
        case Axis::X:
            return context.blockX;
        case Axis::Y:
            return context.blockY;
        case Axis::Z:
            break;
        }
        return context.blockZ;
    }

    [[nodiscard]] double minValue() const override { return -1.0e9; }

    [[nodiscard]] double maxValue() const override { return 1.0e9; }

private:
    Axis axis_;
};

[[nodiscard]] basalt::server::world::densityfunction::Function probe(CoordinateProbe::Axis axis) {
    return std::make_shared<const CoordinateProbe>(axis);
}

/// A router whose surface sits at a known height, so the two height questions are answerable.
[[nodiscard]] OverworldNoiseRouter router() {
    const basalt::server::world::densityfunction::Function zero = df::zero();
    return OverworldNoiseRouter{.barrierNoise = zero,
                                .fluidLevelFloodednessNoise = zero,
                                .fluidLevelSpreadNoise = zero,
                                .lavaNoise = zero,
                                .shiftX = zero,
                                .shiftZ = zero,
                                .temperature = zero,
                                .vegetation = zero,
                                .continents = zero,
                                .erosion = zero,
                                .depth = zero,
                                .ridges = zero,
                                .preliminarySurfaceLevel = df::constant(61.5),
                                // Solid up to y 70 and air above it.
                                .finalDensity = df::yClampedGradient(70, 71, 1.0, -1.0),
                                .veinToggle = zero,
                                .veinRidged = zero,
                                .veinGap = zero};
}

/// A jar that holds one noise file per name, and counts how often each was read.
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
        // The test files carry only their first octave, which is all a parser needs to differ by.
        return NormalNoise::Parameters{.firstOctave = std::stoi(std::string{json}), .amplitudes = {1.0}};
    };
}

[[nodiscard]] std::string noiseEntry(const std::string& name) {
    return "data/minecraft/worldgen/noise/" + name + ".json";
}

} // namespace

TEST_CASE("the world's own seed for the biome zoom comes back as it was handed in") {
    const OverworldNoiseRouter built = router();
    FakeJar jar;
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2), built,
                                             12345};

    CHECK(environment.biomeZoomSeed() == 12345);
    // parity: the whole-world environment cannot see a neighbouring column, so nothing is steep.
    CHECK_FALSE(environment.isSteep(0, 0));
    CHECK_FALSE(environment.isSteep(1000, -1000));
}

TEST_CASE("the surface height is the first solid block found from the top down") {
    const OverworldNoiseRouter built = router();
    FakeJar jar;
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2), built,
                                             0};

    CHECK(environment.estimatedSurfaceHeight(8, 8) == 71);
    // The preliminary level is the router's own answer, floored.
    CHECK(environment.preliminarySurfaceLevel(8, 8) == 61);
}

TEST_CASE("the preliminary level is asked for the quart the column belongs to") {
    FakeJar jar;
    // The environment borrows the router, so each one has to outlive its own environment.
    OverworldNoiseRouter byXRouter = router();
    byXRouter.preliminarySurfaceLevel = probe(CoordinateProbe::Axis::X);
    OverworldNoiseRouter byYRouter = router();
    byYRouter.preliminarySurfaceLevel = probe(CoordinateProbe::Axis::Y);
    OverworldNoiseRouter byZRouter = router();
    byZRouter.preliminarySurfaceLevel = probe(CoordinateProbe::Axis::Z);
    const PositionalRandomFactory factory = PositionalRandomFactory::xoroshiro(1, 2);
    const RealSurfaceEnvironment byX{jar.readText(), parser(), factory, byXRouter, 0};
    const RealSurfaceEnvironment byY{jar.readText(), parser(), factory, byYRouter, 0};
    const RealSurfaceEnvironment byZ{jar.readText(), parser(), factory, byZRouter, 0};

    // parity: both horizontal coordinates are quantized to their quart before the router is asked,
    // and a negative one rounds down rather than towards zero.
    CHECK(byX.preliminarySurfaceLevel(8, 999) == 8);
    CHECK(byX.preliminarySurfaceLevel(11, 999) == 8);
    CHECK(byX.preliminarySurfaceLevel(12, 999) == 12);
    CHECK(byX.preliminarySurfaceLevel(-1, 999) == -4);
    CHECK(byX.preliminarySurfaceLevel(-4, 999) == -4);
    CHECK(byX.preliminarySurfaceLevel(-5, 999) == -8);

    CHECK(byZ.preliminarySurfaceLevel(999, 8) == 8);
    CHECK(byZ.preliminarySurfaceLevel(999, 11) == 8);
    CHECK(byZ.preliminarySurfaceLevel(999, 12) == 12);
    CHECK(byZ.preliminarySurfaceLevel(999, -1) == -4);

    // The height it is asked at is always zero, whatever column it is.
    CHECK(byY.preliminarySurfaceLevel(8, 8) == 0);
    CHECK(byY.preliminarySurfaceLevel(-100, 100) == 0);
}

TEST_CASE("the surface scan stops on the first block that is solid, not merely not air") {
    FakeJar jar;
    OverworldNoiseRouter routed = router();
    // Zero exactly at y 71, positive below it: the scan treats zero as nothing to stand on.
    routed.finalDensity = df::yClampedGradient(70, 72, 1.0, -1.0);
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2), routed,
                                             0};

    CHECK(environment.estimatedSurfaceHeight(8, 8) == 71);
}

TEST_CASE("a column with nothing solid in it answers with the floor of the world") {
    FakeJar jar;
    OverworldNoiseRouter routed = router();
    routed.finalDensity = df::zero();
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2), routed,
                                             0};

    CHECK(environment.estimatedSurfaceHeight(8, 8) == -64);
}

TEST_CASE("a noise the jar does not ship reads as flat zero rather than failing") {
    const OverworldNoiseRouter built = router();
    FakeJar jar;
    // A seed whose first draw at this column is high enough to be visible under the quarter scale.
    const PositionalRandomFactory factory = PositionalRandomFactory::xoroshiro(1, 2);
    const RealSurfaceEnvironment environment{jar.readText(), parser(), factory, built, 0};

    CHECK(environment.sample2d("nothing_of_the_sort", 8, 8) == doctest::Approx(0.0));
    CHECK(environment.sample3d("nothing_of_the_sort", 8, 8, 8) == doctest::Approx(0.0));
    CHECK(environment.surfaceSecondaryNoise(8, 8) == doctest::Approx(0.0));
    // parity: the depth still gets its three offsets, plus a jitter drawn at this very column.
    const std::int32_t depth = environment.surfaceDepthNoise(8, 8);
    CHECK(depth == 3);
    const double expected = (0.0 * 2.75) + 3.0 + (factory.at(8, 0, 8).nextDouble() * 0.25);
    CHECK(depth == static_cast<std::int32_t>(expected));
    CHECK(expected > 3.0);
}

TEST_CASE("each named noise is the one the jar shipped under that name") {
    const OverworldNoiseRouter built = router();
    FakeJar jar{.files = {{noiseEntry("surface"), "-6"}, {noiseEntry("surface_secondary"), "-3"}}};
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(7, 9), built,
                                             0};

    // The two are different noises, so they do not answer the same thing at the same place.
    const double first = environment.sample2d("surface", 100, 200);
    const double second = environment.surfaceSecondaryNoise(100, 200);
    CHECK(first != doctest::Approx(second));
    // And each is read from the jar once, whatever it is asked afterwards.
    CHECK(environment.sample2d("surface", 300, 400) != doctest::Approx(first));
    CHECK(jar.reads[noiseEntry("surface")] == 1);

    // A scaled sample at whole coordinates is the same as the plain one.
    CHECK(environment.sample2dScaled("surface", 100.0, 200.0) == doctest::Approx(first));
    // And at a fraction it is not, which is the whole point of taking doubles.
    CHECK(environment.sample2dScaled("surface", 100.5, 200.0) != doctest::Approx(first));
}

TEST_CASE("the vertical gradient random is one factory per name and depends on the position") {
    const OverworldNoiseRouter built = router();
    FakeJar jar;
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(3, 4), built,
                                             0};

    const float bedrockFloor = environment.verticalGradientRandom("bedrock_floor", 8, -60, 8);
    CHECK(bedrockFloor >= 0.0F);
    CHECK(bedrockFloor < 1.0F);
    // The same name at the same place is the same draw however often it is asked.
    CHECK(environment.verticalGradientRandom("bedrock_floor", 8, -60, 8) == doctest::Approx(bedrockFloor));
    // Another position, and another name, are not.
    CHECK(environment.verticalGradientRandom("bedrock_floor", 8, -59, 8) != doctest::Approx(bedrockFloor));
    CHECK(environment.verticalGradientRandom("bedrock_roof", 8, -60, 8) != doctest::Approx(bedrockFloor));
}

TEST_CASE("the positional random is the world's own, at the block asked for") {
    const OverworldNoiseRouter built = router();
    FakeJar jar;
    const PositionalRandomFactory factory = PositionalRandomFactory::xoroshiro(11, 13);
    const RealSurfaceEnvironment environment{jar.readText(), parser(), factory, built, 0};

    RandomSource expected = factory.at(4, 5, 6);
    RandomSource actual = environment.positionalRandom(4, 5, 6);
    CHECK(actual.nextInt() == expected.nextInt());
    CHECK(actual.nextLong() == expected.nextLong());
}

TEST_CASE("a biome name is handed out by reference and the same one every time") {
    const OverworldNoiseRouter built = router();
    FakeJar jar;
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2), built,
                                             0};

    const std::string& first = environment.biomeAt(8, 64, 8);
    CHECK_FALSE(first.empty());
    // The same name comes back at the same address, which is what the interface promises.
    CHECK(&environment.biomeAt(8, 64, 8) == &first);
    CHECK(&environment.noiseBiomeAt(2, 16, 2) == &environment.noiseBiomeAt(2, 16, 2));
    CHECK(environment.biomeAt(8, 64, 8, nullptr) == first);
}

TEST_CASE("the clay bands are the badlands table, read at the height asked for") {
    const OverworldNoiseRouter built = router();
    FakeJar jar;
    const RealSurfaceEnvironment environment{jar.readText(), parser(), PositionalRandomFactory::xoroshiro(1, 2), built,
                                             0};

    // parity: 192 bands, so two heights that far apart share one and a nearby pair need not.
    const std::int32_t low = environment.clayBand(8, 0, 8);
    CHECK(low == environment.clayBand(8, 192, 8));
    CHECK(low == environment.clayBand(9, 0, 9));
    bool anyDifferent = false;
    for (std::int32_t y = 1; y < 192; ++y) {
        anyDifferent = anyDifferent || environment.clayBand(8, y, 8) != low;
    }
    CHECK(anyDifferent);
}

TEST_CASE("the depth jitter is drawn at the column itself, at height zero") {
    const OverworldNoiseRouter built = router();
    FakeJar jar{.files = {{noiseEntry("surface"), "-6"}}};
    const PositionalRandomFactory factory = PositionalRandomFactory::xoroshiro(5, 6);
    const RealSurfaceEnvironment environment{jar.readText(), parser(), factory, built, 0};

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
