#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/DoubleChests.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t chest(const std::string& facing, const std::string& type,
                                 const std::string& path = "chest") {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), {{"facing", facing}, {"type", type}, {"waterlogged", "false"}},
        std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

} // namespace

TEST_CASE("the right half leads whichever one is asked") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // A chest facing north has its left to the west, so the left half stands there.
    REQUIRE(world.setBlockState(7, 64, 8, chest("north", "left")));
    REQUIRE(world.setBlockState(8, 64, 8, chest("north", "right")));

    const std::optional<std::pair<doubleChests::Half, doubleChests::Half>> fromRight =
        doubleChests::halves(world, 8, 64, 8);
    REQUIRE(fromRight.has_value());
    CHECK(fromRight->first == doubleChests::Half{8, 64, 8});
    CHECK(fromRight->second == doubleChests::Half{7, 64, 8});

    // Asked from the other half the answer is the same pair in the same order.
    const std::optional<std::pair<doubleChests::Half, doubleChests::Half>> fromLeft =
        doubleChests::halves(world, 7, 64, 8);
    REQUIRE(fromLeft.has_value());
    CHECK(fromLeft->first == doubleChests::Half{8, 64, 8});
    CHECK(fromLeft->second == doubleChests::Half{7, 64, 8});
}

TEST_CASE("the partner is found on whichever side the facing puts it") {
    const auto pairedAlong = [](const std::string& facing, std::int32_t dx, std::int32_t dz) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, chest(facing, "right")));
        REQUIRE(world.setBlockState(8 + dx, 64, 8 + dz, chest(facing, "left")));
        const std::optional<std::pair<doubleChests::Half, doubleChests::Half>> pair =
            doubleChests::halves(world, 8, 64, 8);
        return pair.has_value() && pair->second == doubleChests::Half{8 + dx, 64, 8 + dz};
    };

    // A right half looks counter-clockwise from its facing for the left one.
    CHECK(pairedAlong("north", -1, 0));
    CHECK(pairedAlong("south", 1, 0));
    CHECK(pairedAlong("east", 0, -1));
    CHECK(pairedAlong("west", 0, 1));
}

TEST_CASE("a single chest is no pair") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, chest("north", "single")));

    CHECK_FALSE(doubleChests::halves(world, 8, 64, 8).has_value());
}

TEST_CASE("a half whose partner is gone or wrong is no pair") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, chest("north", "right")));

    // Nothing beside it.
    CHECK_FALSE(doubleChests::halves(world, 8, 64, 8).has_value());

    // A chest facing another way.
    REQUIRE(world.setBlockState(7, 64, 8, chest("south", "left")));
    CHECK_FALSE(doubleChests::halves(world, 8, 64, 8).has_value());

    // A chest that claims the same half.
    REQUIRE(world.setBlockState(7, 64, 8, chest("north", "right")));
    CHECK_FALSE(doubleChests::halves(world, 8, 64, 8).has_value());

    // A chest of another kind.
    REQUIRE(world.setBlockState(7, 64, 8, chest("north", "left", "trapped_chest")));
    CHECK_FALSE(doubleChests::halves(world, 8, 64, 8).has_value());
}

TEST_CASE("copper chests pair like the wooden ones, and other blocks do not pair at all") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, chest("north", "right", "waxed_exposed_copper_chest")));
    REQUIRE(world.setBlockState(7, 64, 8, chest("north", "left", "waxed_exposed_copper_chest")));

    CHECK(doubleChests::halves(world, 8, 64, 8).has_value());

    REQUIRE(world.setBlockState(4, 64, 4, registry::blocks::named("stone")->stateId));
    CHECK_FALSE(doubleChests::halves(world, 4, 64, 4).has_value());
    CHECK_FALSE(doubleChests::halves(world, 300, 64, 300).has_value());
}
