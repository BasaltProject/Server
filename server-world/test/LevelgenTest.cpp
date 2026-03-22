#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/densityfunction/DensityFunctionJson.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <doctest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <string_view>
#include <vector>

using namespace basalt::server;
using namespace basalt::server::world;

namespace {

// Vectors dumped from the official 26.3 server as raw float bits.
constexpr std::array<std::array<double, 3>, 4> kPoints = {
    {{0.5, 64.25, -13.75}, {1023.5, -40.0, 777.125}, {-5000.0, 12.5, 3333.3}, {17.0, 200.0, -9.5}}};

void checkNoise(const noise::Noise& sampler, const std::array<std::uint32_t, 4>& expected) {
    for (std::size_t index = 0; index < kPoints.size(); ++index) {
        const auto& point = kPoints[index];
        CHECK(std::bit_cast<std::uint32_t>(sampler.get(point[0], point[1], point[2])) == expected[index]);
    }
}

} // namespace

TEST_CASE("a parity normal noise matches the official server bit for bit") {
    random::RandomSource source = random::RandomSource::xoroshiro(12345);
    const std::array<double, 4> amplitudes = {1.0, 1.0, 0.0, 1.0};
    checkNoise(*noise::NormalNoise::createParity(-7, amplitudes).create(source),
               {0x3c936032U, 0xbbf331a2U, 0x3e748feeU, 0x3eaf20f3U});
}

TEST_CASE("a normal noise from file parameters matches the official server bit for bit") {
    random::RandomSource source = random::RandomSource::xoroshiro(99);
    const noise::NormalNoise recipe{{.baseAmplitude = 1.0,
                                     .baseOctave = -3,
                                     .octaveCount = 3,
                                     .normalize = noise::NormalNoise::Normalization::Legacy,
                                     .amplitudeModifiers = {1.0, 0.5, 1.0}}};
    checkNoise(*recipe.create(source), {0x3d882fdaU, 0xbf04ff08U, 0x3cbb6a34U, 0x3e3ea7c9U});
}

TEST_CASE("the legacy nether biome noise matches the official server bit for bit") {
    random::RandomSource source = random::RandomSource::legacy(7);
    const std::array<double, 2> amplitudes = {1.0, 1.0};
    checkNoise(*noise::NormalNoise::createParity(-7, amplitudes).createForLegacyNetherBiome(source),
               {0x3d949b27U, 0xbf1a33bbU, 0x3f15dbe2U, 0x3dd1d1a7U});
}

TEST_CASE("the blended noise matches the official server bit for bit") {
    random::RandomSource source = random::RandomSource::xoroshiro(42);
    const densityfunction::Sampler blended =
        densityfunction::functions::compileBlendedNoise(source, 0.25, 0.125, 80.0, 160.0, 8.0);
    densityfunction::SamplerContext context{densityfunction::SamplerContext::Options{}};
    const std::array<std::array<std::int32_t, 3>, 4> blocks = {
        {{0, 0, 0}, {100, 64, -200}, {-4000, -30, 5000}, {7, 250, 13}}};
    const std::array<std::uint32_t, 4> expected = {0xbc347568U, 0xbd843ed9U, 0x3e8dcffcU, 0xbf0fa925U};
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& block = blocks[index];
        CHECK(std::bit_cast<std::uint32_t>(blended->sampleValue(context, block[0], block[1], block[2])) ==
              expected[index]);
    }
}

TEST_CASE("a noise file reads the fields 26.3 writes and rejects what the jar's codec rejects") {
    const auto parse = [](std::string_view text) {
        const auto json = basalt::wire::common::parseJson(text);
        REQUIRE(json.has_value());
        return densityfunction::parseNoiseParameters(*json);
    };
    const auto full = parse(
        R"({"base_amplitude": 0.5, "base_octave": -7, "octave_count": 3, "normalize": "legacy", "amplitude_modifiers": [1, 0, 0.5]})");
    REQUIRE(full.has_value());
    CHECK(full->parameters().baseAmplitude == 0.5);
    CHECK(full->parameters().baseOctave == -7);
    CHECK(full->parameters().octaveCount == 3);
    CHECK(full->parameters().normalize == noise::NormalNoise::Normalization::Legacy);
    CHECK(full->parameters().amplitudeModifiers == std::vector<double>{1.0, 0.0, 0.5});

    const auto defaults = parse(R"({"base_octave": -3})");
    REQUIRE(defaults.has_value());
    CHECK(defaults->parameters().baseAmplitude == 1.0);
    CHECK(defaults->parameters().octaveCount == 1);
    CHECK(defaults->parameters().normalize == noise::NormalNoise::Normalization::Enabled);

    CHECK_FALSE(parse(R"({"octave_count": 2})").has_value());
    CHECK_FALSE(parse(R"({"base_octave": 0, "octave_count": 2, "amplitude_modifiers": [1]})").has_value());
}
