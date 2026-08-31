#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/spline/CubicSpline.hpp>

#include <doctest.h>

using namespace basalt::server::world::spline;

namespace {

/// The coordinate is just the float itself, which is what the terrain splines mostly do.
[[nodiscard]] BoundedFloatFunction<float> identity(float min, float max) {
    return {[](const float& value) { return value; }, min, max};
}

} // namespace

TEST_CASE("a constant spline ignores where it is sampled") {
    const ConstantSpline<float> spline{2.5F};
    CHECK(spline.sample(-100.0F) == 2.5F);
    CHECK(spline.sample(100.0F) == 2.5F);
    CHECK(spline.minValue() == 2.5F);
    CHECK(spline.maxValue() == 2.5F);
}

TEST_CASE("a spline passes through every point it was given") {
    std::vector<SplineRef<float>> values;
    values.push_back(constantSpline<float>(0.0F));
    values.push_back(constantSpline<float>(1.0F));
    values.push_back(constantSpline<float>(4.0F));
    const auto spline = MultipointSpline<float>::create(identity(-1.0F, 3.0F), {0.0F, 1.0F, 2.0F}, std::move(values),
                                                        {0.0F, 0.0F, 0.0F});
    CHECK(spline->sample(0.0F) == doctest::Approx(0.0F));
    CHECK(spline->sample(1.0F) == doctest::Approx(1.0F));
    CHECK(spline->sample(2.0F) == doctest::Approx(4.0F));
    // Between two points with zero tangents it is the smooth step between them, not the line.
    const float middle = spline->sample(0.5F);
    CHECK(middle > 0.0F);
    CHECK(middle < 1.0F);
}

TEST_CASE("past its ends a spline carries on along the endpoint's own tangent") {
    std::vector<SplineRef<float>> values;
    values.push_back(constantSpline<float>(10.0F));
    values.push_back(constantSpline<float>(20.0F));
    const auto spline =
        MultipointSpline<float>::create(identity(-10.0F, 10.0F), {0.0F, 1.0F}, std::move(values), {2.0F, 3.0F});
    // Two below the first location, along a tangent of two.
    CHECK(spline->sample(-2.0F) == doctest::Approx(10.0F - 4.0F));
    // Two above the last, along a tangent of three.
    CHECK(spline->sample(3.0F) == doctest::Approx(20.0F + 6.0F));

    // A zero tangent means it simply stops.
    std::vector<SplineRef<float>> flat;
    flat.push_back(constantSpline<float>(7.0F));
    const auto held = MultipointSpline<float>::create(identity(-10.0F, 10.0F), {0.0F}, std::move(flat), {0.0F});
    CHECK(held->sample(-5.0F) == 7.0F);
    CHECK(held->sample(5.0F) == 7.0F);
}

TEST_CASE("the bounds cover what the spline can actually reach") {
    std::vector<SplineRef<float>> values;
    values.push_back(constantSpline<float>(0.0F));
    values.push_back(constantSpline<float>(1.0F));
    const auto spline =
        MultipointSpline<float>::create(identity(-1.0F, 2.0F), {0.0F, 1.0F}, std::move(values), {1.0F, 1.0F});
    // Sampling anywhere in range must stay inside what the bounds promised.
    for (float at = -1.0F; at <= 2.0F; at += 0.05F) {
        CAPTURE(at);
        const float value = spline->sample(at);
        REQUIRE(value >= spline->minValue());
        REQUIRE(value <= spline->maxValue());
    }
}

TEST_CASE("a spline of splines samples through its children") {
    std::vector<SplineRef<float>> inner;
    inner.push_back(constantSpline<float>(1.0F));
    inner.push_back(constantSpline<float>(3.0F));
    SplineRef<float> nested =
        MultipointSpline<float>::create(identity(0.0F, 1.0F), {0.0F, 1.0F}, std::move(inner), {0.0F, 0.0F});

    std::vector<SplineRef<float>> outer;
    outer.push_back(std::move(nested));
    outer.push_back(constantSpline<float>(9.0F));
    const auto spline =
        MultipointSpline<float>::create(identity(0.0F, 1.0F), {0.0F, 1.0F}, std::move(outer), {0.0F, 0.0F});
    CHECK(spline->sample(0.0F) == doctest::Approx(1.0F));
    CHECK(spline->sample(1.0F) == doctest::Approx(9.0F));
}

TEST_CASE("ragged or empty input is refused rather than sampled") {
    std::vector<SplineRef<float>> one;
    one.push_back(constantSpline<float>(0.0F));
    CHECK_THROWS_AS(static_cast<void>(MultipointSpline<float>::create(identity(0.0F, 1.0F), {0.0F, 1.0F},
                                                                      std::move(one), {0.0F, 0.0F})),
                    std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(MultipointSpline<float>::create(identity(0.0F, 1.0F), {}, {}, {})),
                    std::invalid_argument);
}
