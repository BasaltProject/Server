#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/ImprovedNoise.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/noise/PerlinNoise.hpp>
#include <basalt/server/world/noise/SimplexNoise.hpp>

#include <doctest.h>

#include <bit>
#include <cstdint>
#include <vector>

// Golden vectors dumped from the frozen Kotlin server, which is the second parity oracle next to
// the official jar. Compared as raw bit patterns: noise feeds worldgen, so "close enough" is not a
// pass, and a printed decimal would hide the last few bits.
using namespace basalt::server::random;
using namespace basalt::server::world::noise;

namespace {

std::int64_t bits(double value) { return std::bit_cast<std::int64_t>(value); }

} // namespace

TEST_CASE("ImprovedNoise matches the frozen oracle") {
    RandomSource source = RandomSource::xoroshiro(42);
    const ImprovedNoise noise{source};
    CHECK(bits(noise.xo()) == 4640918272274675899LL);
    CHECK(bits(noise.yo()) == 4636870059310201404LL);
    CHECK(bits(noise.zo()) == 4639528240469996357LL);

    const std::vector<std::int64_t> expected = {-4632965614192757386LL, 4597095899294858564LL, -4627442113777264928LL,
                                                4593275514016519560LL,  4591927705686652600LL, -4644104573491720384LL};
    for (int i = 0; i < 6; ++i) {
        CHECK(bits(noise.noise(i * 1.7, i * 0.3 + 2.0, i * -2.9)) == expected[static_cast<std::size_t>(i)]);
    }
}

TEST_CASE("PerlinNoise matches the frozen oracle") {
    RandomSource source = RandomSource::xoroshiro(7);
    const std::vector<double> amplitudes = {1.0, 1.0, 1.0, 1.0};
    const PerlinNoise noise = PerlinNoise::create(source, -4, amplitudes);

    const std::vector<std::int64_t> expected = {4592341896712393172LL, 4593552143334245774LL, 4599341053331970908LL,
                                                4589420502979998257LL, 4596839821472314267LL, -4628072960177627276LL};
    for (int i = 0; i < 6; ++i) {
        CHECK(bits(noise.getValue(i * 13.5, i * 4.25, i * -7.75)) == expected[static_cast<std::size_t>(i)]);
    }
    CHECK(bits(noise.maxValue()) == 4611686018427387904LL);
}

TEST_CASE("NormalNoise matches the frozen oracle") {
    RandomSource source = RandomSource::xoroshiro(123456789);
    const std::vector<double> amplitudes = {1.0, 1.0, 1.0};
    const NormalNoise noise = NormalNoise::create(source, -3, amplitudes);

    const std::vector<std::int64_t> expected = {-4622851185577241790LL, 4603265092902841074LL, -4623944669024874848LL,
                                                -4620121673263212321LL, 4594539285441967825LL, -4622662083385483278LL};
    for (int i = 0; i < 6; ++i) {
        CHECK(bits(noise.getValue(i * 11.0, i * 3.0, i * -5.0)) == expected[static_cast<std::size_t>(i)]);
    }
    CHECK(bits(noise.maxValue()) == 4617315517961601024LL);
}

TEST_CASE("SimplexNoise matches the frozen oracle") {
    RandomSource source = RandomSource::xoroshiro(99);
    const SimplexNoise noise{source};

    const std::vector<std::int64_t> expected2d = {0LL, 4599204859759169726LL, 4601823992331037121LL,
                                                  4602639860275100911LL};
    const std::vector<std::int64_t> expected3d = {0LL, -4625435633073906348LL, 4587430750247611132LL,
                                                  -4617177307883974077LL};
    for (int i = 0; i < 4; ++i) {
        CHECK(bits(noise.getValue(i * 1.5, i * -2.5)) == expected2d[static_cast<std::size_t>(i)]);
        CHECK(bits(noise.getValue(i * 1.5, i * -2.5, i * 0.75)) == expected3d[static_cast<std::size_t>(i)]);
    }
}
