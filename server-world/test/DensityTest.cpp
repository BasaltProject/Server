#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/densityfunction/DensityCache.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <doctest.h>

#include <array>
#include <bit>
#include <cmath>

using namespace basalt::server::world::densityfunction;
namespace fn = basalt::server::world::densityfunction::functions;

namespace {

[[nodiscard]] double at(const Function& function, std::int32_t x, std::int32_t y, std::int32_t z,
                        DensityCache* cache = nullptr) {
    FunctionContext context{x, y, z, cache};
    return function->compute(context);
}

} // namespace

TEST_CASE("arithmetic folds its bounds from its arguments") {
    const Function two = fn::constant(2.0);
    const Function three = fn::constant(3.0);
    CHECK(at(fn::add(two, three), 0, 0, 0) == 5.0);
    CHECK(at(fn::mul(two, three), 0, 0, 0) == 6.0);
    CHECK(at(fn::min(two, three), 0, 0, 0) == 2.0);
    CHECK(at(fn::max(two, three), 0, 0, 0) == 3.0);

    // The bounds are not decoration: min and max short-circuit on them.
    const Function ramp = fn::yClampedGradient(0, 10, -1.0, 1.0);
    CHECK(ramp->minValue() == -1.0);
    CHECK(ramp->maxValue() == 1.0);
    const Function product = fn::mul(ramp, fn::constant(-2.0));
    CHECK(product->minValue() == -2.0);
    CHECK(product->maxValue() == 2.0);
}

TEST_CASE("a multiply by an exact zero never evaluates its other side") {
    // parity: vanilla short-circuits this, and with it whatever the other side would have cost.
    bool touched = false;

    struct Watcher final : public DensityFunction {
        explicit Watcher(bool& flag) : flag_(&flag) {}

        [[nodiscard]] double compute(FunctionContext&) const override {
            *flag_ = true;
            return 1.0;
        }

        [[nodiscard]] double minValue() const override { return 0.0; }

        [[nodiscard]] double maxValue() const override { return 1.0; }

        bool* flag_;
    };

    const Function watched = std::make_shared<Watcher>(touched);
    CHECK(at(fn::mul(fn::constant(0.0), watched), 0, 0, 0) == 0.0);
    CHECK_FALSE(touched);
}

TEST_CASE("a gradient ramps between its two heights and clamps outside them") {
    const Function ramp = fn::yClampedGradient(0, 100, 0.0, 1.0);
    CHECK(at(ramp, 0, -50, 0) == 0.0);
    CHECK(at(ramp, 0, 0, 0) == 0.0);
    CHECK(at(ramp, 0, 50, 0) == doctest::Approx(0.5));
    CHECK(at(ramp, 0, 100, 0) == 1.0);
    CHECK(at(ramp, 0, 500, 0) == 1.0);
}

TEST_CASE("the unary transforms are the ones vanilla names") {
    CHECK(transform(MappedType::Abs, -3.0) == 3.0);
    CHECK(transform(MappedType::Square, -3.0) == 9.0);
    CHECK(transform(MappedType::Cube, -3.0) == -27.0);
    CHECK(transform(MappedType::HalfNegative, -4.0) == -2.0);
    CHECK(transform(MappedType::HalfNegative, 4.0) == 4.0);
    CHECK(transform(MappedType::QuarterNegative, -4.0) == -1.0);
    CHECK(transform(MappedType::Invert, 4.0) == 0.25);
    // Squeeze clamps first, so anything past one is the same as one.
    CHECK(transform(MappedType::Squeeze, 2.0) == doctest::Approx(0.5 - 1.0 / 24.0));

    // An inverted range that straddles zero has no finite bound.
    const Function straddling = fn::invert(fn::yClampedGradient(0, 10, -1.0, 1.0));
    CHECK(std::isinf(straddling->minValue()));
    CHECK(std::isinf(straddling->maxValue()));
}

TEST_CASE("a range choice picks by where its input falls") {
    const Function choice =
        fn::rangeChoice(fn::yClampedGradient(0, 10, 0.0, 10.0), 3.0, 7.0, fn::constant(1.0), fn::constant(-1.0));
    CHECK(at(choice, 0, 0, 0) == -1.0);
    CHECK(at(choice, 0, 3, 0) == 1.0);
    CHECK(at(choice, 0, 6, 0) == 1.0);
    // The upper bound is exclusive.
    CHECK(at(choice, 0, 7, 0) == -1.0);
}

TEST_CASE("an interval select needs one more function than thresholds") {
    const Function select = fn::intervalSelect(fn::yClampedGradient(0, 10, 0.0, 10.0), {2.0, 5.0},
                                               {fn::constant(-1.0), fn::constant(0.0), fn::constant(1.0)});
    CHECK(at(select, 0, 0, 0) == -1.0);
    CHECK(at(select, 0, 3, 0) == 0.0);
    CHECK(at(select, 0, 8, 0) == 1.0);

    CHECK_THROWS_AS(static_cast<void>(fn::intervalSelect(fn::constant(0.0), {1.0}, {fn::constant(0.0)})),
                    std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(fn::intervalSelect(fn::constant(0.0), {5.0, 1.0},
                                                         {fn::constant(0.0), fn::constant(1.0), fn::constant(2.0)})),
                    std::invalid_argument);
}

TEST_CASE("a lerp of a constant first argument folds to the cheaper form") {
    const Function alpha = fn::yClampedGradient(0, 10, 0.0, 1.0);
    const Function folded = fn::lerp(alpha, fn::constant(10.0), fn::constant(20.0));
    CHECK(at(folded, 0, 0, 0) == doctest::Approx(10.0));
    CHECK(at(folded, 0, 5, 0) == doctest::Approx(15.0));
    CHECK(at(folded, 0, 10, 0) == doctest::Approx(20.0));
}

TEST_CASE("the flat cache quantises to the quart grid and samples at y zero") {
    // This is not memoisation: reproducing vanilla's quantisation is the point, and it changes the
    // value rather than just the cost.
    const Function position = fn::add(fn::yClampedGradient(0, 1, 0.0, 1000.0), fn::constant(0.0));
    const Function cached = fn::flatCache(position);
    DensityCache cache{0, 0, markerSlotCount()};

    // Inside the chunk it is sampled at y zero however high the query is.
    CHECK(at(cached, 1, 90, 1, &cache) == 0.0);
    // Outside the chunk's own grid it falls through to the real position.
    CHECK(at(cached, 500, 90, 500, &cache) == 1000.0);
}

TEST_CASE("the once cache answers the same position without recomputing it") {
    int calls = 0;

    struct Counter final : public DensityFunction {
        explicit Counter(int& count) : count_(&count) {}

        [[nodiscard]] double compute(FunctionContext& context) const override {
            (*count_)++;
            return context.blockY;
        }

        [[nodiscard]] double minValue() const override { return -1000.0; }

        [[nodiscard]] double maxValue() const override { return 1000.0; }

        int* count_;
    };

    const Function counted = fn::cacheOnce(std::make_shared<Counter>(calls));
    DensityCache cache{0, 0, markerSlotCount()};
    CHECK(at(counted, 1, 5, 1, &cache) == 5.0);
    CHECK(at(counted, 1, 5, 1, &cache) == 5.0);
    CHECK(calls == 1);
    // A different position is a different answer, and costs one more call.
    CHECK(at(counted, 1, 6, 1, &cache) == 6.0);
    CHECK(calls == 2);
}

TEST_CASE("an interpolated function is sampled at cell corners and lerped between them") {
    // A function linear in y interpolates exactly, so the corners and the lerp must agree with it.
    const Function ramp = fn::yClampedGradient(-64, 320, -64.0, 320.0);
    const Function interpolated = fn::interpolated(ramp);
    DensityCache cache{0, 0, markerSlotCount()};
    cache.configureInterpolation(4, 8, -64, 384);
    for (std::int32_t y = 0; y <= 32; ++y) {
        CAPTURE(y);
        CHECK(at(interpolated, 2, y, 2, &cache) == doctest::Approx(static_cast<double>(y)));
    }
}

TEST_CASE("a cache cannot change its cell shape once corners are in it") {
    DensityCache cache{0, 0, markerSlotCount()};
    cache.configureInterpolation(4, 8, -64, 384);
    cache.configureInterpolation(4, 8, -64, 384);
    CHECK_THROWS_AS(cache.configureInterpolation(8, 8, -64, 384), std::invalid_argument);
    CHECK_THROWS_AS(cache.configureInterpolation(0, 8, -64, 384), std::invalid_argument);
}

TEST_CASE("the blended noise matches the frozen kotlin bit for bit") {
    // Bit patterns, not decimals: a decimal hides the last few bits and worldgen notices them.
    // These come from the Kotlin oracle, printed with toRawBits.
    constexpr std::array<std::int64_t, 8> kExpected = {
        4592239787576482707, 4589726119636153599, -4624255616494697514, -4625649593899731623,
        4597102279051544562, 4589200778692081536, 4598672606353026824,  -4631687159608971012,
    };
    const Function blended = fn::blendedNoiseUnseeded(0.25, 0.125, 80.0, 160.0, 8.0);
    CHECK(std::bit_cast<std::int64_t>(blended->maxValue()) == 4635861306042740639);
    for (std::size_t i = 0; i < kExpected.size(); ++i) {
        CAPTURE(i);
        const auto step = static_cast<std::int32_t>(i);
        const double value = at(blended, step * 37, step * 11 - 40, step * -23);
        CHECK(std::bit_cast<std::int64_t>(value) == kExpected[i]);
    }
}

TEST_CASE("the end islands match the frozen kotlin bit for bit") {
    // This one is float arithmetic throughout on purpose: its rounding is part of the shape, not an
    // implementation detail, so widening it to double would move the islands.
    constexpr std::array<std::int64_t, 8> kExpected = {
        4603241769126068224,  -4617596992938311680, -4617596992938311680, 4597261677853933568,
        -4646192198000312320, 4602397344195936256,  4580160821035794432,  -4626752353532379136,
    };
    const Function islands = fn::endIslands(1234567890);
    for (std::size_t i = 0; i < kExpected.size(); ++i) {
        CAPTURE(i);
        const auto step = static_cast<std::int32_t>(i);
        const double value = at(islands, step * 640, 64, step * -448);
        CHECK(std::bit_cast<std::int64_t>(value) == kExpected[i]);
    }
}

TEST_CASE("finding the top surface walks down in cell steps") {
    // Positive above sixty four, negative below it: the scan must land on the first cell boundary
    // at or under the upper bound where the density is still positive.
    const Function density = fn::yClampedGradient(63, 65, -1.0, 1.0);
    const Function surface = fn::findTopSurface(density, fn::constant(128.0), -64, 8);
    CHECK(at(surface, 0, 0, 0) == 128.0);

    // An upper bound below the floor gives the floor back rather than scanning.
    const Function shallow = fn::findTopSurface(density, fn::constant(-100.0), -64, 8);
    CHECK(at(shallow, 0, 0, 0) == -64.0);

    // A density that is never positive gives the floor too.
    const Function never = fn::findTopSurface(fn::constant(-1.0), fn::constant(128.0), -64, 8);
    CHECK(at(never, 0, 0, 0) == -64.0);
}
