#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/SnowyBlocks.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
using basalt::server::world::Chunk;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::string snowyOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("snowy");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

} // namespace

TEST_CASE("what counts as snow on top") {
    CHECK(snowyBlocks::isSnow(stateOf("snow")));
    CHECK(snowyBlocks::isSnow(stateOf("snow_block")));
    CHECK(snowyBlocks::isSnow(stateOf("powder_snow")));
    CHECK_FALSE(snowyBlocks::isSnow(stateOf("ice")));
    CHECK_FALSE(snowyBlocks::isSnow(stateOf("white_wool")));
    CHECK_FALSE(snowyBlocks::isSnow(registry::blocks::air().stateId));
}

TEST_CASE("which blocks wear it") {
    CHECK(snowyBlocks::wearsSnow(stateOf("grass_block")));
    CHECK(snowyBlocks::wearsSnow(stateOf("podzol")));
    CHECK(snowyBlocks::wearsSnow(stateOf("mycelium")));
    CHECK_FALSE(snowyBlocks::wearsSnow(stateOf("dirt")));
    CHECK_FALSE(snowyBlocks::wearsSnow(stateOf("stone")));
    // A block with properties of its own but not that one: having a state to vary is not the same
    // as wearing snow.
    CHECK_FALSE(snowyBlocks::wearsSnow(stateOf("oak_log")));
    CHECK_FALSE(snowyBlocks::wearsSnow(stateOf("snow")));
    CHECK_FALSE(snowyBlocks::wearsSnow(registry::blocks::air().stateId));
}

TEST_CASE("grass turns white under snow and back when it goes") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(2, 64, 2, stateOf("grass_block")));
    CHECK(snowyOf(*world.blockState(2, 64, 2)) == "false");
    // Nothing above it, so nothing to change.
    CHECK_FALSE(snowyBlocks::updated(world, 2, 64, 2).has_value());

    REQUIRE(world.setBlockState(2, 65, 2, stateOf("snow")));
    const std::optional<std::int32_t> white = snowyBlocks::updated(world, 2, 64, 2);
    REQUIRE(white.has_value());
    CHECK(snowyOf(*white) == "true");
    // The answer is the block it should be, not a block that was written: writing it is the
    // caller's, which is what lets one pass collect every change first.
    CHECK(snowyOf(*world.blockState(2, 64, 2)) == "false");

    REQUIRE(world.setBlockState(2, 64, 2, *white));
    // Now it already looks right, so there is nothing to report.
    CHECK_FALSE(snowyBlocks::updated(world, 2, 64, 2).has_value());

    // parity: breaking the snow takes the white off again, which is the half a naive port misses.
    REQUIRE(world.setBlockState(2, 65, 2, registry::blocks::air().stateId));
    const std::optional<std::int32_t> green = snowyBlocks::updated(world, 2, 64, 2);
    REQUIRE(green.has_value());
    CHECK(snowyOf(*green) == "false");
    CHECK(*green == stateOf("grass_block"));
}

TEST_CASE("blocks that do not wear snow are left alone, loaded or not") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("dirt")));
    REQUIRE(world.setBlockState(4, 65, 4, stateOf("snow")));
    CHECK_FALSE(snowyBlocks::updated(world, 4, 64, 4).has_value());

    // Nothing there at all.
    CHECK_FALSE(snowyBlocks::updated(world, 500, 64, 500).has_value());
    // At the top of the world there is no block above to read, so nothing is decided. Asked of a
    // block that is already white, that has to stay white rather than being cleared by a question
    // nobody could answer.
    REQUIRE(world.setBlockState(4, 319, 4, stateOf("grass_block")));
    CHECK_FALSE(snowyBlocks::updated(world, 4, 319, 4).has_value());

    const std::optional<std::int32_t> whiteGrass = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("grass_block"), {{"snowy", "true"}}, stateOf("grass_block"));
    REQUIRE(whiteGrass.has_value());
    REQUIRE(world.setBlockState(5, 319, 5, *whiteGrass));
    CHECK_FALSE(snowyBlocks::updated(world, 5, 319, 5).has_value());
}
