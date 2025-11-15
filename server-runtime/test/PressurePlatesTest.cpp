#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState oneChunk() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

} // namespace

TEST_CASE("which blocks are plates") {
    CHECK(pressurePlates::isPlate(stateOf("stone_pressure_plate")));
    CHECK(pressurePlates::isPlate(stateOf("oak_pressure_plate")));
    CHECK(pressurePlates::isPlate(stateOf("light_weighted_pressure_plate")));
    CHECK(pressurePlates::isPlate(stateOf("heavy_weighted_pressure_plate")));
    CHECK_FALSE(pressurePlates::isPlate(stateOf("stone")));
    CHECK_FALSE(pressurePlates::isPlate(registry::blocks::air().stateId));
    CHECK_FALSE(pressurePlates::isPlate(stateOf("stone_button")));
}

TEST_CASE("standing on a plain plate presses it, and leaving lets it up") {
    WorldState world = oneChunk();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(3, 64, 3, stateOf("stone_pressure_plate")));

    const std::optional<BlockChange> pressed = pressurePlates::update(world, 3, 64, 3, 1);
    REQUIRE(pressed.has_value());
    CHECK(pressed->x == 3);
    CHECK(pressed->y == 64);
    CHECK(pressed->z == 3);
    CHECK(propertyOf(pressed->stateId, "powered") == "true");
    CHECK(world.blockState(3, 64, 3) == pressed->stateId);
    // A pressed plate books its own turn, which is what checks later whether anything is still on
    // it: without that it would stay down for ever.
    CHECK(world.hasScheduledBlockTick(3, 64, 3, Identifier::vanilla("stone_pressure_plate")));

    // Still standing on it: nothing changed, so nothing is reported.
    CHECK_FALSE(pressurePlates::update(world, 3, 64, 3, 4).has_value());

    const std::optional<BlockChange> released = pressurePlates::update(world, 3, 64, 3, 0);
    REQUIRE(released.has_value());
    CHECK(propertyOf(released->stateId, "powered") == "false");
    CHECK_FALSE(pressurePlates::update(world, 3, 64, 3, 0).has_value());
}

TEST_CASE("a weighted plate counts what is on it against its own limit") {
    WorldState world = oneChunk();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(1, 64, 1, stateOf("light_weighted_pressure_plate")));

    // parity: rounded up, so one item already reads one rather than nothing.
    const std::optional<BlockChange> one = pressurePlates::update(world, 1, 64, 1, 1);
    REQUIRE(one.has_value());
    CHECK(propertyOf(one->stateId, "power") == "1");

    const std::optional<BlockChange> eight = pressurePlates::update(world, 1, 64, 1, 8);
    REQUIRE(eight.has_value());
    CHECK(propertyOf(eight->stateId, "power") == "8");

    // Its limit is fifteen, and a hundred items on it read as fifteen rather than as a hundred,
    // which is not a state a plate has.
    const std::optional<BlockChange> full = pressurePlates::update(world, 1, 64, 1, 100);
    REQUIRE(full.has_value());
    CHECK(propertyOf(full->stateId, "power") == "15");
    CHECK_FALSE(pressurePlates::update(world, 1, 64, 1, 15).has_value());

    // The heavy one spreads a hundred and fifty over the same fifteen levels.
    REQUIRE(world.setBlockState(2, 64, 1, stateOf("heavy_weighted_pressure_plate")));
    const std::optional<BlockChange> heavyOne = pressurePlates::update(world, 2, 64, 1, 1);
    REQUIRE(heavyOne.has_value());
    CHECK(propertyOf(heavyOne->stateId, "power") == "1");
    // Ten of a hundred and fifty is exactly one level, so it is still showing one and there is
    // nothing to report: the rounding up only adds a level once something is left over.
    CHECK_FALSE(pressurePlates::update(world, 2, 64, 1, 10).has_value());
    const std::optional<BlockChange> heavyEleven = pressurePlates::update(world, 2, 64, 1, 11);
    REQUIRE(heavyEleven.has_value());
    CHECK(propertyOf(heavyEleven->stateId, "power") == "2");
}

TEST_CASE("a weighted plate books a shorter turn than a plain one") {
    WorldState world = oneChunk();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(5, 64, 5, stateOf("light_weighted_pressure_plate")));
    static_cast<void>(pressurePlates::update(world, 5, 64, 5, 1));

    // Ten ticks, not twenty: it comes due on the tenth call and not before.
    for (std::int32_t at = 0; at < 9; ++at) {
        CAPTURE(at);
        CHECK(world.dueBlockTicks().empty());
    }
    CHECK(world.dueBlockTicks().size() == 1);
}

TEST_CASE("what is not a plate, and what is not there") {
    WorldState world = oneChunk();
    // No chunk at all.
    CHECK_FALSE(pressurePlates::update(world, 0, 64, 0, 1).has_value());

    world.getOrGenerateChunk(0, 0);
    // A chunk, but the block is not a plate.
    REQUIRE(world.setBlockState(0, 64, 0, stateOf("stone")));
    CHECK_FALSE(pressurePlates::update(world, 0, 64, 0, 1).has_value());
}

TEST_CASE("the box that presses a plate is narrower than the block") {
    // parity: fourteen sixteenths across, so brushing the very edge does not press it.
    CHECK(pressurePlates::isStandingOn(0, 64, 0, 0.5, 64.0, 0.5));
    CHECK(pressurePlates::isStandingOn(0, 64, 0, 0.0625, 64.0, 0.9375));
    CHECK_FALSE(pressurePlates::isStandingOn(0, 64, 0, 0.0, 64.0, 0.5));
    CHECK_FALSE(pressurePlates::isStandingOn(0, 64, 0, 1.0, 64.0, 0.5));
    CHECK_FALSE(pressurePlates::isStandingOn(0, 64, 0, 0.5, 64.0, 0.0));
    CHECK_FALSE(pressurePlates::isStandingOn(0, 64, 0, 0.5, 64.0, 1.0));

    // A quarter of a block tall, and nothing below it counts.
    CHECK(pressurePlates::isStandingOn(0, 64, 0, 0.5, 64.25, 0.5));
    CHECK_FALSE(pressurePlates::isStandingOn(0, 64, 0, 0.5, 64.3, 0.5));
    CHECK_FALSE(pressurePlates::isStandingOn(0, 64, 0, 0.5, 63.9, 0.5));

    // And it is measured from the plate's own coordinates, wherever they are.
    CHECK(pressurePlates::isStandingOn(-5, 10, 7, -4.5, 10.0, 7.5));
    CHECK_FALSE(pressurePlates::isStandingOn(-5, 10, 7, -5.0, 10.0, 7.5));
}

TEST_CASE("the block under a thing is the one its feet are in") {
    CHECK(pressurePlates::blockUnder(64.0) == 64);
    CHECK(pressurePlates::blockUnder(64.9) == 64);
    // Below zero the floor rounds down, not towards zero: standing at -0.5 is in block minus one.
    CHECK(pressurePlates::blockUnder(-0.5) == -1);
    CHECK(pressurePlates::blockUnder(-1.0) == -1);
}
