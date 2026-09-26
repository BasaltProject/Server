#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/VanillaWorldGeneratorFactory.hpp>

#include <doctest.h>

#include <string>

using basalt::server::runtime::world::VanillaWorldGeneratorFactory;
using basalt::server::runtime::world::WorldMetadata;
namespace config = basalt::server::runtime::config;

TEST_CASE("a dimension without the jar's worldgen data says so instead of generating nothing") {
    VanillaWorldGeneratorFactory factory{{}};
    const VanillaWorldGeneratorFactory::Generators overworld = factory.create(
        config::WorldDeclaration{
            .name = "world", .dimension = config::WorldDimension::Overworld, .type = config::WorldType::Normal},
        WorldMetadata{});
    // A dimension's shape is the jar's own noise settings. Answering an unset generator is what
    // lets the caller fall back; a generator built on missing noise would crash on its first chunk.
    CHECK_FALSE(overworld.pipeline.terrain);
    CHECK(overworld.villages == nullptr);

    const VanillaWorldGeneratorFactory::Generators nether = factory.create(
        config::WorldDeclaration{
            .name = "nether", .dimension = config::WorldDimension::Nether, .type = config::WorldType::Normal},
        WorldMetadata{});
    CHECK_FALSE(nether.pipeline.terrain);

    const VanillaWorldGeneratorFactory::Generators end = factory.create(
        config::WorldDeclaration{
            .name = "end", .dimension = config::WorldDimension::End, .type = config::WorldType::Normal},
        WorldMetadata{});
    CHECK_FALSE(end.pipeline.terrain);

    // Nothing was built, so nothing is cached.
    CHECK(factory.cacheStats().assemblies == 0);
}
