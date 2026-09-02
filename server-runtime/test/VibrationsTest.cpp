#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <doctest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::int32_t sleepingSensor() {
    return stateOf("sculk_sensor", {{"sculk_sensor_phase", "inactive"}, {"power", "0"}, {"waterlogged", "false"}});
}

const basalt::wire::common::Identifier kSensor = basalt::wire::common::Identifier::vanilla("sculk_sensor");

} // namespace

TEST_CASE("every event carries the frequency vanilla gives it") {
    // The whole table, since a wrong number here is a sensor that reads the wrong signal and nothing
    // else would catch it.
    const std::vector<std::pair<std::string, std::int32_t>> table{
        {"step", 1},
        {"swim", 1},
        {"flap", 1},
        {"projectile_land", 2},
        {"hit_ground", 2},
        {"splash", 2},
        {"bounce", 2},
        {"item_interact_finish", 3},
        {"projectile_shoot", 3},
        {"instrument_play", 3},
        {"entity_action", 4},
        {"elytra_glide", 4},
        {"unequip", 4},
        {"entity_dismount", 5},
        {"equip", 5},
        {"entity_interact", 6},
        {"shear", 6},
        {"entity_mount", 6},
        {"entity_damage", 7},
        {"drink", 8},
        {"eat", 8},
        {"container_close", 9},
        {"block_close", 9},
        {"block_deactivate", 9},
        {"block_detach", 9},
        {"container_open", 10},
        {"block_open", 10},
        {"block_activate", 10},
        {"block_attach", 10},
        {"prime_fuse", 10},
        {"note_block_play", 10},
        {"block_change", 11},
        {"block_destroy", 12},
        {"fluid_pickup", 12},
        {"block_place", 13},
        {"fluid_place", 13},
        {"entity_place", 14},
        {"lightning_strike", 14},
        {"teleport", 14},
        {"entity_die", 15},
        {"explode", 15},
    };
    for (const auto& [event, frequency] : table) {
        CHECK(vibrations::frequencyOf(event) == frequency);
    }
    CHECK(table.size() == 41);

    // An event nothing listens to is worth nothing, and so is one that does not exist.
    CHECK(vibrations::frequencyOf("block_update") == 0);
    CHECK(vibrations::frequencyOf("") == 0);
}

TEST_CASE("a sensor wakes on the event's own frequency and books its thirty ticks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, sleepingSensor()));
    std::vector<BlockChange> changes;

    vibrations::emit(world, "eat", 10.5, 64.5, 8.5, changes);

    REQUIRE(changes.size() == 1);
    CHECK(changes[0].x == 8);
    CHECK(propertyOf(changes[0].stateId, "sculk_sensor_phase") == "active");
    CHECK(propertyOf(changes[0].stateId, "power") == "8");
    CHECK(world.hasScheduledBlockTick(8, 64, 8, kSensor));
}

TEST_CASE("a sensor already awake takes no second vibration") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, sleepingSensor()));
    std::vector<BlockChange> changes;

    vibrations::emit(world, "eat", 8.5, 64.5, 8.5, changes);
    vibrations::emit(world, "explode", 8.5, 64.5, 8.5, changes);

    CHECK(changes.size() == 1);
    CHECK(propertyOf(*world.blockState(8, 64, 8), "power") == "8");
}

TEST_CASE("an event nothing listens to reaches no sensor") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, sleepingSensor()));
    std::vector<BlockChange> changes;

    vibrations::emit(world, "block_update", 8.5, 64.5, 8.5, changes);

    CHECK(changes.empty());
    CHECK(world.blockState(8, 64, 8) == sleepingSensor());
}

TEST_CASE("the reach is a sphere of eight, not the cube it is walked in") {
    const auto hears = [](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        WorldState world = emptyWorld();
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                world.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
        REQUIRE(world.setBlockState(8 + dx, 64 + dy, 8 + dz, sleepingSensor()));
        std::vector<BlockChange> changes;
        vibrations::emit(world, "eat", 8.5, 64.5, 8.5, changes);
        return !changes.empty();
    };

    CHECK(hears(8, 0, 0));
    CHECK(hears(-8, 0, 0));
    CHECK(hears(0, 8, 0));
    CHECK(hears(0, -8, 0));
    CHECK(hears(0, 0, 8));
    CHECK(hears(0, 0, -8));
    CHECK(hears(4, 4, 4));
    // The corner of the cube is thirteen away, well past the radius.
    CHECK_FALSE(hears(8, 8, 8));
    CHECK_FALSE(hears(6, 6, 0));
    CHECK_FALSE(hears(9, 0, 0));
}

TEST_CASE("a calibrated sensor listens the same way") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t calibrated =
        stateOf("calibrated_sculk_sensor",
                {{"sculk_sensor_phase", "inactive"}, {"power", "0"}, {"waterlogged", "false"}, {"facing", "north"}});
    REQUIRE(world.setBlockState(8, 64, 8, calibrated));
    std::vector<BlockChange> changes;

    vibrations::emit(world, "step", 8.5, 64.5, 8.5, changes);

    REQUIRE(changes.size() == 1);
    CHECK(propertyOf(changes[0].stateId, "sculk_sensor_phase") == "active");
    CHECK(propertyOf(changes[0].stateId, "power") == "1");
    // The face it was placed on is not touched.
    CHECK(propertyOf(changes[0].stateId, "facing") == "north");
    CHECK(world.hasScheduledBlockTick(8, 64, 8, basalt::wire::common::Identifier::vanilla("calibrated_sculk_sensor")));
}

TEST_CASE("an awake sensor settles through cooldown and then goes quiet") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, sleepingSensor()));
    std::vector<BlockChange> changes;
    vibrations::emit(world, "eat", 8.5, 64.5, 8.5, changes);

    // The thirty ticks it stays awake.
    for (std::int32_t turn = 0; turn < 29; ++turn) {
        CHECK(world.dueBlockTicks().empty());
    }
    CHECK(world.dueBlockTicks().size() == 1);

    const std::optional<BlockChange> cooling = vibrations::tick(world, 8, 64, 8);
    REQUIRE(cooling.has_value());
    CHECK(propertyOf(cooling->stateId, "sculk_sensor_phase") == "cooldown");
    CHECK(propertyOf(cooling->stateId, "power") == "0");

    // And the ten it takes to settle.
    for (std::int32_t turn = 0; turn < 9; ++turn) {
        CHECK(world.dueBlockTicks().empty());
    }
    CHECK(world.dueBlockTicks().size() == 1);

    const std::optional<BlockChange> quiet = vibrations::tick(world, 8, 64, 8);
    REQUIRE(quiet.has_value());
    CHECK(propertyOf(quiet->stateId, "sculk_sensor_phase") == "inactive");
    CHECK(world.blockState(8, 64, 8) == sleepingSensor());

    // Once quiet there is nothing left to book or to change.
    CHECK_FALSE(vibrations::tick(world, 8, 64, 8).has_value());
    for (std::int32_t turn = 0; turn < 40; ++turn) {
        CHECK(world.dueBlockTicks().empty());
    }
}

TEST_CASE("a scheduled turn on anything but a sensor does nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::named("stone")->stateId));

    CHECK_FALSE(vibrations::tick(world, 8, 64, 8).has_value());
    CHECK_FALSE(vibrations::tick(world, 8, 400, 8).has_value());
}

TEST_CASE("the source block is the one the position falls in, negative or not") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(-1, -1);
    REQUIRE(world.setBlockState(-9, 64, -9, sleepingSensor()));
    std::vector<BlockChange> changes;

    // Truncating instead of flooring would put the source at -0.5 into block zero, eight away.
    vibrations::emit(world, "eat", -0.5, 64.5, -0.5, changes);

    CHECK(changes.empty());
}

TEST_CASE("the cheap filter callers use is the cube, and it is eight either way") {
    CHECK(vibrations::isInRange(0, 0, 0, 8.0, 0.0, 0.0));
    CHECK(vibrations::isInRange(0, 0, 0, -8.0, 0.0, 0.0));
    CHECK(vibrations::isInRange(0, 0, 0, 8.0, 8.0, 8.0));
    CHECK_FALSE(vibrations::isInRange(0, 0, 0, 8.5, 0.0, 0.0));
    CHECK_FALSE(vibrations::isInRange(0, 0, 0, 0.0, -8.5, 0.0));
    CHECK_FALSE(vibrations::isInRange(0, 0, 0, 0.0, 0.0, 9.0));
}
