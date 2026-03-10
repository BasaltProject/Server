#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PressurePlates.hpp>

#include <doctest.h>

#include <cstdint>
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

    const std::optional<BlockChange> pressed =
        pressurePlates::update(world, 3, 64, 3, pressurePlates::Trigger::Touched, 1);
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
    CHECK_FALSE(pressurePlates::update(world, 3, 64, 3, pressurePlates::Trigger::Scheduled, 4).has_value());

    const std::optional<BlockChange> released =
        pressurePlates::update(world, 3, 64, 3, pressurePlates::Trigger::Scheduled, 0);
    REQUIRE(released.has_value());
    CHECK(propertyOf(released->stateId, "powered") == "false");
    CHECK_FALSE(pressurePlates::update(world, 3, 64, 3, pressurePlates::Trigger::Scheduled, 0).has_value());
}

TEST_CASE("a plate that is already on stops counting until its turn comes") {
    WorldState world = oneChunk();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("light_weighted_pressure_plate")));

    // parity: `entityInside` only asks while the plate reads zero. Four items landing on the same
    // tick are ticked one at a time, so the first one in presses it and the plate stops looking:
    // measured in `placas`, where the jar reads one for ten ticks and four after that.
    const std::optional<BlockChange> first =
        pressurePlates::update(world, 4, 64, 4, pressurePlates::Trigger::Touched, 1);
    REQUIRE(first.has_value());
    CHECK(propertyOf(first->stateId, "power") == "1");
    CHECK_FALSE(pressurePlates::update(world, 4, 64, 4, pressurePlates::Trigger::Touched, 4).has_value());

    // Its own turn is the one that counts them all.
    const std::optional<BlockChange> later =
        pressurePlates::update(world, 4, 64, 4, pressurePlates::Trigger::Scheduled, 4);
    REQUIRE(later.has_value());
    CHECK(propertyOf(later->stateId, "power") == "4");
}

TEST_CASE("a weighted plate counts what is on it against its own limit") {
    WorldState world = oneChunk();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(1, 64, 1, stateOf("light_weighted_pressure_plate")));

    // parity: rounded up, so one item already reads one rather than nothing.
    const std::optional<BlockChange> one = pressurePlates::update(world, 1, 64, 1, pressurePlates::Trigger::Touched, 1);
    REQUIRE(one.has_value());
    CHECK(propertyOf(one->stateId, "power") == "1");

    const std::optional<BlockChange> eight =
        pressurePlates::update(world, 1, 64, 1, pressurePlates::Trigger::Scheduled, 8);
    REQUIRE(eight.has_value());
    CHECK(propertyOf(eight->stateId, "power") == "8");

    // Its limit is fifteen, and a hundred items on it read as fifteen rather than as a hundred,
    // which is not a state a plate has.
    const std::optional<BlockChange> full =
        pressurePlates::update(world, 1, 64, 1, pressurePlates::Trigger::Scheduled, 100);
    REQUIRE(full.has_value());
    CHECK(propertyOf(full->stateId, "power") == "15");
    CHECK_FALSE(pressurePlates::update(world, 1, 64, 1, pressurePlates::Trigger::Scheduled, 15).has_value());

    // The heavy one spreads a hundred and fifty over the same fifteen levels.
    REQUIRE(world.setBlockState(2, 64, 1, stateOf("heavy_weighted_pressure_plate")));
    const std::optional<BlockChange> heavyOne =
        pressurePlates::update(world, 2, 64, 1, pressurePlates::Trigger::Touched, 1);
    REQUIRE(heavyOne.has_value());
    CHECK(propertyOf(heavyOne->stateId, "power") == "1");
    // Ten of a hundred and fifty is exactly one level, so it is still showing one and there is
    // nothing to report: the rounding up only adds a level once something is left over.
    CHECK_FALSE(pressurePlates::update(world, 2, 64, 1, pressurePlates::Trigger::Scheduled, 10).has_value());
    const std::optional<BlockChange> heavyEleven =
        pressurePlates::update(world, 2, 64, 1, pressurePlates::Trigger::Scheduled, 11);
    REQUIRE(heavyEleven.has_value());
    CHECK(propertyOf(heavyEleven->stateId, "power") == "2");
}

TEST_CASE("a weighted plate books a shorter turn than a plain one") {
    WorldState world = oneChunk();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(5, 64, 5, stateOf("light_weighted_pressure_plate")));
    static_cast<void>(pressurePlates::update(world, 5, 64, 5, pressurePlates::Trigger::Touched, 1));

    // Ten ticks, not twenty: it comes due on the tenth call and not before.
    for (std::int32_t at = 0; at < 9; ++at) {
        CAPTURE(at);
        world.openTurn();
        CHECK(world.dueBlockTicks().empty());
    }
    world.openTurn();
    CHECK(world.dueBlockTicks().size() == 1);
}

TEST_CASE("what is not a plate, and what is not there") {
    WorldState world = oneChunk();
    // No chunk at all.
    CHECK_FALSE(pressurePlates::update(world, 0, 64, 0, pressurePlates::Trigger::Touched, 1).has_value());

    world.getOrGenerateChunk(0, 0);
    // A chunk, but the block is not a plate.
    REQUIRE(world.setBlockState(0, 64, 0, stateOf("stone")));
    CHECK_FALSE(pressurePlates::update(world, 0, 64, 0, pressurePlates::Trigger::Touched, 1).has_value());
}

TEST_CASE("a plate is pressed by a box, not by a point") {
    // parity: `TOUCH_AABB = Block.column(14.0, 0.0, 4.0)`, crossed with `AABB.intersects`. A player
    // is 0.6 wide and 1.8 tall, so half a width is 0.3.
    constexpr double kHalf = 0.3;
    constexpr double kTall = 1.8;

    // Dead centre, and the two ends of the box.
    CHECK(pressurePlates::touches(0, 64, 0, 0.5, 64.0, 0.5, kHalf, kTall));
    CHECK(pressurePlates::touches(0, 64, 0, 0.0625, 64.0, 0.9375, kHalf, kTall));

    // The whole point of the change: a navel over the next square still presses this one, because
    // the body reaches back into it. The old point test called both of these a miss.
    CHECK(pressurePlates::touches(0, 64, 0, 1.1, 64.0, 0.5, kHalf, kTall));
    CHECK(pressurePlates::touches(0, 64, 0, -0.1, 64.0, 0.5, kHalf, kTall));

    // And far enough away it really is a miss: the box ends at a sixteenth from the edge, so a body
    // whose own edge stops short of that touches nothing.
    CHECK_FALSE(pressurePlates::touches(0, 64, 0, 1.4, 64.0, 0.5, kHalf, kTall));
    CHECK_FALSE(pressurePlates::touches(0, 64, 0, 0.5, 64.0, -0.4, kHalf, kTall));

    // `AABB.intersects` is strict, so the boundary is exact: the plate's box ends at 0.9375 and half
    // a body is 0.3, so 1.2375 is the first place that no longer touches it.
    CHECK(pressurePlates::touches(0, 64, 0, 1.2, 64.0, 0.5, kHalf, kTall));
    CHECK_FALSE(pressurePlates::touches(0, 64, 0, 1.2375, 64.0, 0.5, kHalf, kTall));

    // A quarter of a block tall, measured from the plate's own floor. Feet above it miss; a body
    // standing in it reaches down into it.
    CHECK(pressurePlates::touches(0, 64, 0, 0.5, 64.0, 0.5, kHalf, kTall));
    CHECK_FALSE(pressurePlates::touches(0, 64, 0, 0.5, 64.25, 0.5, kHalf, kTall));
    CHECK(pressurePlates::touches(0, 64, 0, 0.5, 63.9, 0.5, kHalf, kTall));

    // And it is measured from the plate's own coordinates, wherever they are.
    CHECK(pressurePlates::touches(-5, 10, 7, -4.5, 10.0, 7.5, kHalf, kTall));
    CHECK_FALSE(pressurePlates::touches(-5, 10, 7, -6.5, 10.0, 7.5, kHalf, kTall));
}

TEST_CASE("the block under a thing is the one its feet are in") {
    CHECK(pressurePlates::blockUnder(64.0) == 64);
    CHECK(pressurePlates::blockUnder(64.9) == 64);
    // Below zero the floor rounds down, not towards zero: standing at -0.5 is in block minus one.
    CHECK(pressurePlates::blockUnder(-0.5) == -1);
    CHECK(pressurePlates::blockUnder(-1.0) == -1);
}
