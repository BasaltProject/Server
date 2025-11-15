#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/SimpleWorldGenerators.hpp>

#include <doctest.h>

using namespace basalt::server::world;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const char* path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

} // namespace

TEST_CASE("a flat world is four layers and then sky") {
    const Chunk chunk = flatWorldGenerator::generate(3, -4);
    CHECK(chunk.chunkX() == 3);
    CHECK(chunk.chunkZ() == -4);

    // parity: the default flat preset, read back off a real 26.2 world.
    CHECK(chunk.blockStateId(0, -64, 0) == stateOf("bedrock"));
    CHECK(chunk.blockStateId(0, -63, 0) == stateOf("dirt"));
    CHECK(chunk.blockStateId(0, -62, 0) == stateOf("dirt"));
    CHECK(chunk.blockStateId(0, -61, 0) == stateOf("grass_block"));
    CHECK(chunk.blockStateId(0, -60, 0) == registry::blocks::air().stateId);
    CHECK(chunk.blockStateId(15, 200, 15) == registry::blocks::air().stateId);

    // Every column, not just the one the corner is in.
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            CAPTURE(x);
            CAPTURE(z);
            CHECK(chunk.blockStateId(x, -64, z) == stateOf("bedrock"));
            CHECK(chunk.blockStateId(x, -61, z) == stateOf("grass_block"));
        }
    }

    // The stack sits on the floor of whatever world it is in, not at a fixed height.
    const Chunk nether = flatWorldGenerator::generate(0, 0, ChunkHeight{0, 128});
    CHECK(nether.blockStateId(0, 0, 0) == stateOf("bedrock"));
    CHECK(nether.blockStateId(0, 3, 0) == stateOf("grass_block"));
}

TEST_CASE("a void world is a platform under the spawn and nothing anywhere else") {
    const Chunk spawn = voidWorldGenerator::generate(0, 0);
    // Nine by nine centred on the spawn column, one block under it.
    CHECK(spawn.blockStateId(8, 63, 8) == stateOf("stone"));
    CHECK(spawn.blockStateId(4, 63, 4) == stateOf("stone"));
    CHECK(spawn.blockStateId(12, 63, 12) == stateOf("stone"));
    CHECK(spawn.blockStateId(3, 63, 8) == registry::blocks::air().stateId);
    CHECK(spawn.blockStateId(13, 63, 8) == registry::blocks::air().stateId);
    // The same edge along z, which is a separate bound and was a separate mistake to make.
    CHECK(spawn.blockStateId(8, 63, 3) == registry::blocks::air().stateId);
    CHECK(spawn.blockStateId(8, 63, 13) == registry::blocks::air().stateId);
    CHECK(spawn.blockStateId(8, 64, 8) == registry::blocks::air().stateId);

    // Everywhere else is sky, however close.
    const Chunk east = voidWorldGenerator::generate(1, 0);
    const Chunk south = voidWorldGenerator::generate(0, 1);
    for (std::int32_t along = 0; along < 16; ++along) {
        CAPTURE(along);
        CHECK(east.blockStateId(along, 63, 8) == registry::blocks::air().stateId);
        CHECK(south.blockStateId(8, 63, along) == registry::blocks::air().stateId);
    }

    // A radius of nothing is a world of nothing, which is what a lobby that builds its own asks
    // for.
    const Chunk empty = voidWorldGenerator::generate(0, 0, 8, 64, 8, 0);
    CHECK(empty.blockStateId(8, 63, 8) == registry::blocks::air().stateId);
}

TEST_CASE("a platform that reaches into the next chunk is built in both") {
    // Radius twenty from the spawn column reaches four blocks past the chunk border.
    const Chunk here = voidWorldGenerator::generate(0, 0, 8, 64, 8, 20);
    const Chunk east = voidWorldGenerator::generate(1, 0, 8, 64, 8, 20);
    const Chunk south = voidWorldGenerator::generate(0, 1, 8, 64, 8, 20);
    CHECK(here.blockStateId(0, 63, 0) == stateOf("stone"));
    CHECK(east.blockStateId(0, 63, 8) == stateOf("stone"));
    CHECK(east.blockStateId(12, 63, 8) == stateOf("stone"));
    CHECK(east.blockStateId(13, 63, 8) == registry::blocks::air().stateId);
    CHECK(south.blockStateId(8, 63, 12) == stateOf("stone"));
    CHECK(south.blockStateId(8, 63, 13) == registry::blocks::air().stateId);
}
