#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/terrain/TerrainProvider.hpp>

#include <doctest.h>

#include <array>
#include <bit>
#include <cstdint>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace fn = basalt::server::world::densityfunction::functions;

namespace {

/// A gradient in y, so a sample is a pure function of the position.
[[nodiscard]] Coordinate axis(double from, double to) {
    return fn::splineCoordinate(fn::yClampedGradient(-64, 320, from, to));
}

[[nodiscard]] std::int32_t bits(float value) { return std::bit_cast<std::int32_t>(value); }

} // namespace

TEST_CASE("the overworld splines match the frozen kotlin bit for bit") {
    // A decimal hides the last few bits and terrain notices them, so these are raw patterns from
    // the Kotlin oracle. Three different ramps so the coordinates never coincide.
    const Coordinate continents = axis(-1.1, 1.0);
    const Coordinate erosion = axis(1.0, -1.0);
    const Coordinate ridges = axis(-1.0, 1.0);
    const Coordinate weirdness = axis(0.9, -0.9);

    const TerrainSpline offset = terrainProvider::overworldOffset(continents, erosion, ridges);
    const TerrainSpline factor = terrainProvider::overworldFactor(continents, erosion, weirdness, ridges);
    const TerrainSpline jaggedness = terrainProvider::overworldJaggedness(continents, erosion, weirdness, ridges);

    CHECK(bits(offset->minValue()) == -1096391066);
    CHECK(bits(offset->maxValue()) == 1069547520);
    CHECK(bits(factor->minValue()) == 1059061760);
    CHECK(bits(factor->maxValue()) == 1086953882);
    CHECK(bits(jaggedness->minValue()) == 0);
    CHECK(bits(jaggedness->maxValue()) == 1059145646);

    constexpr std::array<std::int32_t, 10> kOffset = {1026832728, -1100773274, -1100773274, -1107853464, -1107967345,
                                                      1008112499, 1049999097,  1057630389,  1061896516,  1068764983};
    constexpr std::array<std::int32_t, 10> kFactor = {1081920717, 1081920717, 1081920717, 1081920717, 1081920717,
                                                      1082385342, 1086953882, 1086953882, 1086953882, 1086953882};
    constexpr std::array<std::int32_t, 10> kJagged = {0, 0, 0, 0, 0, 0, 0, 957240222, 1046312824, 1058309550};

    for (std::size_t i = 0; i < kOffset.size(); ++i) {
        CAPTURE(i);
        densityfunction::FunctionContext context{0, -64 + static_cast<std::int32_t>(i) * 40, 0, nullptr};
        CHECK(bits(offset->sample(context)) == kOffset[i]);
        CHECK(bits(factor->sample(context)) == kFactor[i]);
        CHECK(bits(jaggedness->sample(context)) == kJagged[i]);
    }
}

TEST_CASE("peaks and valleys folds weirdness into a ridge") {
    CHECK(bits(terrainProvider::peaksAndValleys(0.4F)) == 1045220556);
    CHECK(bits(terrainProvider::peaksAndValleys(0.56666666F)) == 1060320050);
    // Symmetric about zero, which is what makes it a ridge rather than a slope.
    CHECK(terrainProvider::peaksAndValleys(0.3F) == terrainProvider::peaksAndValleys(-0.3F));
}

TEST_CASE("an erosion offset spline stays inside the bounds it declares") {
    const Coordinate erosion = axis(-1.0, 1.0);
    const Coordinate ridges = axis(1.0, -1.0);
    const TerrainSpline spline =
        terrainProvider::buildErosionOffsetSpline(erosion, ridges, -0.1F, 0.03F, 0.1F, 0.7F, 0.01F, -0.03F, true, true);
    for (std::int32_t y = -64; y <= 320; y += 4) {
        CAPTURE(y);
        densityfunction::FunctionContext context{0, y, 0, nullptr};
        const float value = spline->sample(context);
        REQUIRE(value >= spline->minValue());
        REQUIRE(value <= spline->maxValue());
    }
}
