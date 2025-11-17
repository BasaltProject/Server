#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealNoiseParameters.hpp>

#include <doctest.h>

#include <map>
#include <string>
#include <vector>

using basalt::server::random::PositionalRandomFactory;
using basalt::server::random::RandomSource;
using basalt::server::runtime::world::RealNoiseParameters;
using basalt::server::world::noise::NormalNoise;

namespace {

/// The seed every case here derives from, so an expected noise can be built the same way the class
/// builds it instead of being written down.
[[nodiscard]] PositionalRandomFactory factory() { return PositionalRandomFactory::xoroshiro(1234, 5678); }

/// What the not-yet-ported JSON reader would answer, keyed by the text it was handed.
struct FakeJar {
    std::map<std::string, std::string> files;
    std::vector<std::string> requested;
    std::int32_t parses = 0;

    [[nodiscard]] RealNoiseParameters::ReadText readText() {
        return [this](std::string_view name) -> std::optional<std::string> {
            requested.emplace_back(name);
            const auto found = files.find(std::string{name});
            return found == files.end() ? std::nullopt : std::optional{found->second};
        };
    }

    /// `"<firstOctave> <amplitude>..."`, which is all the real schema carries.
    [[nodiscard]] RealNoiseParameters::ParseParameters parse() {
        return [this](std::string_view json) -> std::optional<NormalNoise::Parameters> {
            ++parses;
            if (json.empty()) {
                return std::nullopt;
            }
            NormalNoise::Parameters parameters{.firstOctave = 0, .amplitudes = {}};
            std::size_t at = 0;
            parameters.firstOctave = std::stoi(std::string{json}, &at);
            while (at < json.size()) {
                std::size_t used = 0;
                parameters.amplitudes.push_back(std::stod(std::string{json.substr(at)}, &used));
                at += used;
            }
            return parameters;
        };
    }
};

[[nodiscard]] NormalNoise expected(std::string_view name, std::int32_t firstOctave,
                                   const std::vector<double>& amplitudes) {
    RandomSource source = factory().fromHashOf(name);
    return NormalNoise::create(source, firstOctave, amplitudes);
}

} // namespace

TEST_CASE("a named noise is seeded off its own vanilla name") {
    FakeJar jar;
    jar.files["data/minecraft/worldgen/noise/continentalness.json"] = "-9 1.0 1.0 2.0 2.0";
    RealNoiseParameters noises{jar.readText(), jar.parse(), factory()};

    const NormalNoise* noise = noises.get("continentalness");
    REQUIRE(noise != nullptr);
    CHECK(jar.requested == std::vector<std::string>{"data/minecraft/worldgen/noise/continentalness.json"});

    const NormalNoise reference = expected("minecraft:continentalness", -9, {1.0, 1.0, 2.0, 2.0});
    CHECK(noise->maxValue() == reference.maxValue());
    CHECK(noise->getValue(1.5, 2.5, 3.5) == reference.getValue(1.5, 2.5, 3.5));
    CHECK(noise->getValue(-40.25, 63.0, 17.75) == reference.getValue(-40.25, 63.0, 17.75));

    // The name is what seeds it: the same octaves under another name are another noise.
    const NormalNoise other = expected("continentalness", -9, {1.0, 1.0, 2.0, 2.0});
    CHECK(noise->getValue(1.5, 2.5, 3.5) != other.getValue(1.5, 2.5, 3.5));
}

TEST_CASE("the same noise instance comes back, read and parsed once") {
    FakeJar jar;
    jar.files["data/minecraft/worldgen/noise/offset.json"] = "-3 1.0 1.0 1.0 0.0";
    RealNoiseParameters noises{jar.readText(), jar.parse(), factory()};

    const NormalNoise* first = noises.get("offset");
    REQUIRE(first != nullptr);
    CHECK(noises.get("offset") == first);
    CHECK(noises.get("offset") == first);
    CHECK(jar.requested.size() == 1);
    CHECK(jar.parses == 1);
}

TEST_CASE("two names are two noises") {
    FakeJar jar;
    jar.files["data/minecraft/worldgen/noise/erosion.json"] = "-9 1.0 1.0 0.0 1.0";
    jar.files["data/minecraft/worldgen/noise/ridge.json"] = "-7 1.0 2.0 1.0 0.0";
    RealNoiseParameters noises{jar.readText(), jar.parse(), factory()};

    const NormalNoise* erosion = noises.get("erosion");
    const NormalNoise* ridge = noises.get("ridge");
    REQUIRE(erosion != nullptr);
    REQUIRE(ridge != nullptr);
    CHECK(erosion != ridge);
    CHECK(erosion->getValue(1.5, 2.5, 3.5) ==
          expected("minecraft:erosion", -9, {1.0, 1.0, 0.0, 1.0}).getValue(1.5, 2.5, 3.5));
    CHECK(ridge->getValue(1.5, 2.5, 3.5) ==
          expected("minecraft:ridge", -7, {1.0, 2.0, 1.0, 0.0}).getValue(1.5, 2.5, 3.5));

    // The one already built is still the one that answers.
    CHECK(noises.get("erosion") == erosion);
    CHECK(jar.parses == 2);
}

TEST_CASE("a noise the jar does not ship is nothing, and is asked for again next time") {
    FakeJar jar;
    RealNoiseParameters noises{jar.readText(), jar.parse(), factory()};

    CHECK(noises.get("temperature") == nullptr);
    CHECK(noises.get("temperature") == nullptr);
    CHECK(jar.requested.size() == 2);
    CHECK(jar.parses == 0);
}

TEST_CASE("a file the reader cannot make octaves of is nothing") {
    FakeJar jar;
    jar.files["data/minecraft/worldgen/noise/vegetation.json"] = "";
    RealNoiseParameters noises{jar.readText(), jar.parse(), factory()};

    CHECK(noises.get("vegetation") == nullptr);
    CHECK(jar.parses == 1);
}

TEST_CASE("without a reader or a parser nothing resolves") {
    FakeJar jar;
    jar.files["data/minecraft/worldgen/noise/offset.json"] = "-3 1.0";

    RealNoiseParameters withoutReader{nullptr, jar.parse(), factory()};
    CHECK(withoutReader.get("offset") == nullptr);

    RealNoiseParameters withoutParser{jar.readText(), nullptr, factory()};
    CHECK(withoutParser.get("offset") == nullptr);
    CHECK(jar.requested.empty());
    CHECK(jar.parses == 0);
}
