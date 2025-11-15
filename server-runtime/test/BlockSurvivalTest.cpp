#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

void openSky(WorldState& world) {
    for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            world.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
}

} // namespace

TEST_CASE("a block that needs nothing stands anywhere") {
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    CHECK(blockSurvival::isSupported(world, 8, 64, 8));
    CHECK(blockSurvival::isSupported(world, 8, 70, 8));
}

TEST_CASE("a torch needs something to stand on") {
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("torch")));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8));

    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    CHECK(blockSurvival::isSupported(world, 8, 65, 8));
}

TEST_CASE("a cactus refuses anything beside it") {
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sand")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("cactus")));
    CHECK(blockSurvival::isSupported(world, 8, 65, 8));

    // parity: `CactusBlock.canSurvive` asks what must *not* be beside it, which is a rule a probe
    // of one neighbour at a time cannot express.
    REQUIRE(world.setBlockState(9, 65, 8, stateOf("stone")));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8));
    REQUIRE(world.setBlockState(9, 65, 8, registry::blocks::air().stateId));
    CHECK(blockSurvival::isSupported(world, 8, 65, 8));

    // parity: `CactusBlock.canSurvive` also refuses water over it, which is why a cactus does not
    // grow up into a pond.
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("water")));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8));
    REQUIRE(world.setBlockState(8, 66, 8, registry::blocks::air().stateId));
    CHECK(blockSurvival::isSupported(world, 8, 65, 8));

    // A cactus stands on a cactus, and not on stone.
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("cactus")));
    CHECK(blockSurvival::isSupported(world, 8, 66, 8));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8));
}

TEST_CASE("a hanging spike holds on to the one above it") {
    WorldState world = emptyWorld();
    openSky(world);
    const auto spike = [](const std::string& direction, const std::string& thickness) {
        return stateOf("pointed_dripstone",
                       {{"vertical_direction", direction}, {"thickness", thickness}, {"waterlogged", "false"}});
    };
    REQUIRE(world.setBlockState(8, 70, 8, stateOf("dripstone_block")));
    REQUIRE(world.setBlockState(8, 69, 8, spike("down", "frustum")));
    REQUIRE(world.setBlockState(8, 68, 8, spike("down", "tip")));

    // parity: the first hangs off the sturdy face, the second off the spike above it.
    CHECK(blockSurvival::isSupported(world, 8, 69, 8));
    CHECK(blockSurvival::isSupported(world, 8, 68, 8));

    // A spike pointing the other way is not what holds it.
    REQUIRE(world.setBlockState(8, 69, 8, spike("up", "frustum")));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 68, 8));

    // And with nothing above at all it falls.
    REQUIRE(world.setBlockState(8, 69, 8, registry::blocks::air().stateId));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 68, 8));
}

TEST_CASE("snow stands on a full stack of snow") {
    WorldState world = emptyWorld();
    openSky(world);
    // parity: `SnowLayerBlock.canSurvive`'s last clause. The probe tries each block in its default
    // state, and a default snow layer is one deep, so it never saw this.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("snow", {{"layers", "8"}})));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("snow", {{"layers", "1"}})));
    CHECK(blockSurvival::isSupported(world, 8, 65, 8));

    // One layer short and it has nothing to stand on.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("snow", {{"layers", "7"}})));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8));
}

TEST_CASE("a crop needs the light as well as the soil") {
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("farmland")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("wheat")));
    CHECK(blockSurvival::isSupported(world, 8, 65, 8));

    // parity: `CropBlock.canSurvive` wants light eight over its soil, which is one less than
    // growing needs. A roof takes that away.
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 70, z, stateOf("stone")));
        }
    }
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8));
    // A world with no sky at all is the same darkness.
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8, std::nullopt, {}, false));
}

TEST_CASE("sugar cane stands beside water") {
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sand")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("sugar_cane")));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 65, 8));

    // Water beside the block it stands on is what keeps it, which is a question about the block
    // below and not about the cane itself.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("water")));
    CHECK(blockSurvival::isSupported(world, 8, 65, 8));
}

TEST_CASE("scaffolding is left standing unless somebody knows better") {
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 70, 8, stateOf("scaffolding")));
    // With nobody to walk the row, scaffolding in mid air is left where it is.
    CHECK(blockSurvival::isSupported(world, 8, 70, 8));

    // And with somebody who does know, it falls.
    const blockSurvival::ScaffoldingHasFallen fallen = [](const WorldState&, std::int32_t, std::int32_t, std::int32_t) {
        return true;
    };
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 70, 8, std::nullopt, fallen));
}

TEST_CASE("the state asked about can be one that is not there yet") {
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    // What a placement asks: would this block stand here, before it is written.
    CHECK(blockSurvival::isSupported(world, 8, 65, 8, stateOf("torch")));
    CHECK_FALSE(blockSurvival::isSupported(world, 8, 66, 8, stateOf("torch")));
}
