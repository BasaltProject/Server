#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PoweredRails.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

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

[[nodiscard]] std::int32_t railOf(const std::string& path, const std::string& shape, const std::string& powered) {
    std::map<std::string, std::string> properties{{"shape", shape}, {"waterlogged", "false"}};
    // A plain rail has no `powered` of its own, which is the whole reason these three do.
    if (path != "rail") {
        properties.emplace("powered", powered);
    }
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string poweredAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("powered");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// Lays a run of powered rails along z and lights the first one from a block of redstone.
void layRun(WorldState& world, std::int32_t length) {
    for (std::int32_t at = 0; at < length; ++at) {
        REQUIRE(world.setBlockState(8, 64, 8 + at, railOf("powered_rail", "north_south", "false")));
    }
}

/// Settles the whole run the way a neighbour update would: each rail asked until nothing moves.
void settleRun(WorldState& world, std::int32_t length) {
    for (std::int32_t round = 0; round < length + 2; ++round) {
        for (std::int32_t at = 0; at < length; ++at) {
            static_cast<void>(poweredRails::update(world, "world", 8, 64, 8 + at));
        }
    }
}

} // namespace

TEST_CASE("a rail beside a source turns on and one further away does not") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, railOf("powered_rail", "north_south", "false")));
    CHECK_FALSE(poweredRails::update(world, "world", 8, 64, 8).has_value());

    REQUIRE(world.setBlockState(7, 64, 8, stateOf("redstone_block")));
    const std::optional<BlockChange> on = poweredRails::update(world, "world", 8, 64, 8);
    REQUIRE(on.has_value());
    CHECK(poweredAt(world, 8, 64, 8) == "true");
    // Asked again with nothing changed it says nothing.
    CHECK_FALSE(poweredRails::update(world, "world", 8, 64, 8).has_value());

    REQUIRE(world.setBlockState(7, 64, 8, registry::blocks::air().stateId));
    const std::optional<BlockChange> off = poweredRails::update(world, "world", 8, 64, 8);
    REQUIRE(off.has_value());
    CHECK(poweredAt(world, 8, 64, 8) == "false");
}

TEST_CASE("one lever lights eight rails along the line and not the ninth") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // The run reaches past the chunk border, which is where a real track usually is.
    world.getOrGenerateChunk(0, 1);
    layRun(world, 12);
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("redstone_block")));
    settleRun(world, 12);

    // parity: `findPoweredRailSignal` counts eight along from the rail being asked, so the source
    // rail plus eight more light up and the tenth stays dark.
    for (std::int32_t at = 0; at < 9; ++at) {
        CAPTURE(at);
        CHECK(poweredAt(world, 8, 64, 8 + at) == "true");
    }
    CHECK(poweredAt(world, 8, 64, 8 + 9) == "false");
    CHECK(poweredAt(world, 8, 64, 8 + 11) == "false");
}

TEST_CASE("a rail crossing the line does not carry the run, however powered it is") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A north-south run with an east-west rail across the middle of it, and the source feeding
    // only that crossing rail.
    for (std::int32_t at = 0; at < 5; ++at) {
        REQUIRE(world.setBlockState(8, 64, 8 + at, railOf("powered_rail", "north_south", "false")));
    }
    REQUIRE(world.setBlockState(8, 64, 10, railOf("powered_rail", "east_west", "false")));
    REQUIRE(world.setBlockState(9, 64, 10, stateOf("redstone_block")));
    settleRun(world, 5);

    // parity: `isSameRailWithPower` refuses a rail lying across the line, so the run beside it
    // stays dark even though the crossing rail itself is lit.
    CHECK(poweredAt(world, 8, 64, 10) == "true");
    CHECK(poweredAt(world, 8, 64, 9) == "false");
    CHECK(poweredAt(world, 8, 64, 11) == "false");
    CHECK(poweredAt(world, 8, 64, 8) == "false");

    // The same the other way round: an east-west run crossed by a north-south rail.
    WorldState across = emptyWorld();
    across.getOrGenerateChunk(0, 0);
    for (std::int32_t at = 0; at < 5; ++at) {
        REQUIRE(across.setBlockState(4 + at, 64, 8, railOf("powered_rail", "east_west", "false")));
    }
    REQUIRE(across.setBlockState(6, 64, 8, railOf("powered_rail", "north_south", "false")));
    REQUIRE(across.setBlockState(6, 64, 9, stateOf("redstone_block")));
    for (std::int32_t round = 0; round < 7; ++round) {
        for (std::int32_t at = 0; at < 5; ++at) {
            static_cast<void>(poweredRails::update(across, "world", 4 + at, 64, 8));
        }
    }
    CHECK(poweredAt(across, 6, 64, 8) == "true");
    CHECK(poweredAt(across, 5, 64, 8) == "false");
    CHECK(poweredAt(across, 7, 64, 8) == "false");
}

TEST_CASE("a rail that is off carries nothing, however far the source is behind it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layRun(world, 3);
    // The source feeds the far end of the run, and nothing has settled yet.
    REQUIRE(world.setBlockState(9, 64, 10, stateOf("redstone_block")));

    // parity: `isSameRailWithPower` needs the neighbour to be on already. That is why a run lights
    // one rail per neighbour update instead of all at once.
    CHECK_FALSE(poweredRails::update(world, "world", 8, 64, 8).has_value());
    CHECK(poweredAt(world, 8, 64, 8) == "false");

    // Once the far one is on, the next asks it and lights, and so on back down the line.
    CHECK(poweredRails::update(world, "world", 8, 64, 10).has_value());
    CHECK(poweredRails::update(world, "world", 8, 64, 9).has_value());
    CHECK(poweredRails::update(world, "world", 8, 64, 8).has_value());
}

TEST_CASE("a rail of another kind does not carry the run") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layRun(world, 4);
    // parity: `isSameRailWithPower` asks for the same block, so an activator rail in the middle of
    // a powered run is not part of it.
    REQUIRE(world.setBlockState(8, 64, 10, railOf("activator_rail", "north_south", "false")));
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("redstone_block")));
    settleRun(world, 4);

    CHECK(poweredAt(world, 8, 64, 9) == "true");
    CHECK(poweredAt(world, 8, 64, 11) == "false");
}

TEST_CASE("the run climbs a slope, which leaves the next rail a block up") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A slope going south, and the flat rail it leads to one block higher. The source feeds only
    // the upper rail, so the slope can light only by looking up along its own climb.
    REQUIRE(world.setBlockState(8, 64, 8, railOf("powered_rail", "ascending_south", "false")));
    REQUIRE(world.setBlockState(8, 65, 9, railOf("powered_rail", "north_south", "false")));
    REQUIRE(world.setBlockState(7, 65, 9, stateOf("redstone_block")));

    for (std::int32_t round = 0; round < 4; ++round) {
        static_cast<void>(poweredRails::update(world, "world", 8, 65, 9));
        static_cast<void>(poweredRails::update(world, "world", 8, 64, 8));
    }
    CHECK(poweredAt(world, 8, 65, 9) == "true");
    CHECK(poweredAt(world, 8, 64, 8) == "true");
}

TEST_CASE("what is not a powered rail is left alone") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south", "false")));
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("redstone_block")));
    CHECK_FALSE(poweredRails::update(world, "world", 8, 64, 8).has_value());

    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    CHECK_FALSE(poweredRails::update(world, "world", 9, 64, 8).has_value());
    CHECK_FALSE(poweredRails::update(world, "world", 500, 64, 500).has_value());

    // A detector rail has a `powered` of its own and is driven by carts, not by this: leaving it
    // out of the kinds is what stops a block of redstone switching one on.
    REQUIRE(world.setBlockState(8, 64, 10, railOf("detector_rail", "north_south", "false")));
    REQUIRE(world.setBlockState(9, 64, 10, stateOf("redstone_block")));
    CHECK_FALSE(poweredRails::update(world, "world", 8, 64, 10).has_value());
    CHECK(poweredAt(world, 8, 64, 10) == "false");
}
