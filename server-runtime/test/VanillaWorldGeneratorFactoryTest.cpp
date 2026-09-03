#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/NoiseParametersJson.hpp>
#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>

#include <doctest.h>

#include <string>

using basalt::server::runtime::world::VanillaWorldGeneratorFactory;
using basalt::server::runtime::world::WorldMetadata;
namespace config = basalt::server::runtime::config;
namespace noiseParametersJson = basalt::server::runtime::world::noiseParametersJson;

TEST_CASE("noise parameters read the two fields vanilla writes, and nothing else") {
    const std::optional<basalt::server::world::noise::NormalNoise::Parameters> parsed =
        noiseParametersJson::parse(R"({"firstOctave":-7,"amplitudes":[1.0,1.0,0.0,0.5]})");
    REQUIRE(parsed.has_value());
    CHECK(parsed->firstOctave == -7);
    CHECK(parsed->amplitudes == std::vector<double>{1.0, 1.0, 0.0, 0.5});

    // Integers are numbers too: `[1, 1]` is what several of the real files hold.
    const std::optional<basalt::server::world::noise::NormalNoise::Parameters> whole =
        noiseParametersJson::parse(R"({"firstOctave":-3,"amplitudes":[1,1]})");
    REQUIRE(whole.has_value());
    CHECK(whole->amplitudes == std::vector<double>{1.0, 1.0});

    // A shape this does not understand answers nothing rather than an empty noise.
    CHECK_FALSE(noiseParametersJson::parse(R"({"amplitudes":[1.0]})").has_value());
    CHECK_FALSE(noiseParametersJson::parse(R"({"firstOctave":-7})").has_value());
    CHECK_FALSE(noiseParametersJson::parse("not json at all").has_value());
}

TEST_CASE("a world with no jar still builds a generator that answers chunks") {
    // With no jar every noise is absent, so the terrain is whatever an empty router shapes: the
    // point is that building one and asking it for a chunk works at all, which is what a server
    // with no downloaded assets does on its first tick.
    VanillaWorldGeneratorFactory factory{{}};
    const VanillaWorldGeneratorFactory::Generators end = factory.create(
        config::WorldDeclaration{
            .name = "end", .dimension = config::WorldDimension::End, .type = config::WorldType::Normal},
        WorldMetadata{});
    REQUIRE(end.generate);
    const basalt::server::world::Chunk chunk = end.generate(0, 0, 0, "normal");
    CHECK(chunk.chunkX() == 0);
    CHECK(chunk.chunkZ() == 0);
    // The End has no village generator to name, and nothing to locate a stronghold with.
    CHECK(end.villages == nullptr);
    CHECK(end.strongholds == nullptr);
}

TEST_CASE("an overworld without the jar's noise says so instead of generating nothing") {
    VanillaWorldGeneratorFactory factory{{}};
    const VanillaWorldGeneratorFactory::Generators overworld = factory.create(
        config::WorldDeclaration{
            .name = "world", .dimension = config::WorldDimension::Overworld, .type = config::WorldType::Normal},
        WorldMetadata{});
    // The shape of the overworld is the jar's own noise files. Answering an unset generator is what
    // lets the caller fall back; a generator built on missing noise would crash on its first chunk.
    CHECK_FALSE(overworld.generate);
    CHECK(overworld.villages == nullptr);

    const VanillaWorldGeneratorFactory::Generators nether = factory.create(
        config::WorldDeclaration{
            .name = "nether", .dimension = config::WorldDimension::Nether, .type = config::WorldType::Normal},
        WorldMetadata{});
    CHECK_FALSE(nether.generate);

    // Nothing was built, so nothing is cached.
    CHECK(factory.cacheStats().assemblies == 0);
}
