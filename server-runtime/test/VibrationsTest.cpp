#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
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

/// A world with a clock, because a vibration is not heard where it happens: it takes a tick per
/// block to arrive, and the sensor only moves it on its own scheduled turn.
struct Bench {
    WorldState world = emptyWorld();
    std::int64_t now = 0;
    std::vector<BlockChange> changes;

    /// One turn of the world: the clock moves, every tick that came due goes through the sensor, and
    /// then the block entity's own turn moves whatever noise is on its way, which is where
    /// `VibrationSystem.Ticker.tick` lives.
    void advance() {
        ++now;
        // parity: the tick loop opens the turn before anything runs, and that is the one place the
        // scheduled turns count down.
        world.openTurn();
        for (const std::array<std::int32_t, 3>& at : world.dueBlockTicks()) {
            if (const std::optional<BlockChange> change = vibrations::tick(world, at[0], at[1], at[2], now);
                change.has_value()) {
                changes.push_back(*change);
            }
        }
        for (const std::array<std::int32_t, 3>& at : sensors()) {
            if (const std::optional<BlockChange> change = vibrations::travelTick(world, at[0], at[1], at[2], now);
                change.has_value()) {
                changes.push_back(*change);
            }
        }
    }

    /// Every sculk sensor with a block entity, which is what the world's block entity pass walks.
    [[nodiscard]] std::vector<std::array<std::int32_t, 3>> sensors() const {
        std::vector<std::array<std::int32_t, 3>> found;
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                const Chunk* chunk = world.chunkIfLoaded(chunkX, chunkZ);
                if (chunk == nullptr) {
                    continue;
                }
                for (const basalt::server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                    if (entity.type().path() == "sculk_sensor" || entity.type().path() == "calibrated_sculk_sensor") {
                        found.push_back({(chunk->chunkX() << 4) + entity.localX(), entity.y(),
                                         (chunk->chunkZ() << 4) + entity.localZ()});
                    }
                }
            }
        }
        return found;
    }

    void advance(std::int32_t turns) {
        for (std::int32_t turn = 0; turn < turns; ++turn) {
            advance();
        }
    }

    [[nodiscard]] std::int32_t frequencyAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
        const Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        const basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
        return entity == nullptr ? -1 : entity->data().getInt(std::string{vibrations::kFrequencyKey}, -1);
    }
};

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

TEST_CASE("a sensor wakes on the event's own frequency, one turn per block away") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));

    vibrations::emit(bench.world, "eat", 10.5, 64.5, 8.5, bench.now);

    // parity: `VibrationSelector.chosenCandidate` picks on the turn **after** the noise, and
    // `calculateTravelTimeInTicks` then spends one tick per whole block. Two blocks away is two
    // turns, and nothing happens before that.
    bench.advance();
    CHECK(bench.changes.empty());
    bench.advance();
    REQUIRE(bench.changes.size() == 1);
    CHECK(bench.changes[0].x == 8);
    CHECK(propertyOf(bench.changes[0].stateId, "sculk_sensor_phase") == "active");
    // parity: `power` is **how close** the noise was, not what it was:
    // `VibrationSystem.getRedstoneStrengthForDistance` over the block distance, so two squares away
    // is fifteen less three. What it was goes in the block entity, and only a comparator reads it.
    CHECK(propertyOf(bench.changes[0].stateId, "power") == "12");
    CHECK(bench.frequencyAt(8, 64, 8) == 8);
    CHECK(bench.world.hasScheduledBlockTick(8, 64, 8, kSensor));
}

TEST_CASE("a noise in the sensor's own square arrives on the very next turn") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));

    vibrations::emit(bench.world, "eat", 8.5, 64.5, 8.5, bench.now);

    // parity: `Ticker.tick` spends a turn of the countdown on the same turn it picks, so a travel
    // time of zero lands there and does not wait a second turn.
    bench.advance();
    REQUIRE(bench.changes.size() == 1);
    CHECK(propertyOf(bench.changes[0].stateId, "power") == "15");
}

TEST_CASE("of two noises heard on the same turn the sensor keeps the louder one") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));

    // parity: `VibrationSelector.shouldReplaceVibration`: same turn and same distance, so the higher
    // frequency wins. The sensor is not awake yet, so both are candidates and only one survives.
    vibrations::emit(bench.world, "eat", 8.5, 64.5, 8.5, bench.now);
    vibrations::emit(bench.world, "explode", 8.5, 64.5, 8.5, bench.now);
    bench.advance();

    REQUIRE(bench.changes.size() == 1);
    CHECK(bench.frequencyAt(8, 64, 8) == 15);
}

TEST_CASE("a sensor already awake takes no second vibration") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));

    vibrations::emit(bench.world, "eat", 8.5, 64.5, 8.5, bench.now);
    bench.advance();
    REQUIRE(bench.changes.size() == 1);

    vibrations::emit(bench.world, "explode", 8.5, 64.5, 8.5, bench.now);
    bench.advance(5);

    CHECK(bench.changes.size() == 1);
    CHECK(bench.frequencyAt(8, 64, 8) == 8);
}

TEST_CASE("a sensor does not hear its own square being placed or broken") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));

    // parity: `SculkSensorBlockEntity.VibrationUser.canReceiveVibration`, the one event it refuses.
    vibrations::emit(bench.world, "block_place", 8.5, 64.5, 8.5, bench.now);
    vibrations::emit(bench.world, "block_destroy", 8.5, 64.5, 8.5, bench.now);
    bench.advance(4);
    CHECK(bench.changes.empty());

    // The very same event one square over does reach it.
    vibrations::emit(bench.world, "block_place", 9.5, 64.5, 8.5, bench.now);
    bench.advance(3);
    CHECK(bench.changes.size() == 1);
}

TEST_CASE("an event nothing listens to reaches no sensor") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));

    vibrations::emit(bench.world, "block_update", 8.5, 64.5, 8.5, bench.now);
    bench.advance(4);

    CHECK(bench.changes.empty());
    CHECK(bench.world.blockState(8, 64, 8) == sleepingSensor());
}

TEST_CASE("the reach is a sphere of eight, not the cube it is walked in") {
    const auto hears = [](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        Bench bench;
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                bench.world.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
        REQUIRE(bench.world.setBlockState(8 + dx, 64 + dy, 8 + dz, sleepingSensor()));
        vibrations::emit(bench.world, "eat", 8.5, 64.5, 8.5, bench.now);
        bench.advance(12);
        return !bench.changes.empty();
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

TEST_CASE("wool in the line hides a noise, and wool beside it hides nothing") {
    const auto wool = [] { return stateOf("white_wool", {}); };

    SUBCASE("one block in the way is enough") {
        Bench bench;
        bench.world.getOrGenerateChunk(0, 0);
        REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));
        REQUIRE(bench.world.setBlockState(9, 64, 8, wool()));

        vibrations::emit(bench.world, "eat", 10.5, 64.5, 8.5, bench.now);
        bench.advance(6);

        // parity: `isOccluded` wants all six lines blocked, and with the nudge at a hundred
        // thousandth all six cross the same square. Wool between the two is wool between the two.
        CHECK(bench.changes.empty());
    }

    SUBCASE("wool out of the line changes nothing") {
        Bench bench;
        bench.world.getOrGenerateChunk(0, 0);
        REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));
        REQUIRE(bench.world.setBlockState(9, 64, 9, wool()));
        REQUIRE(bench.world.setBlockState(9, 65, 8, wool()));

        vibrations::emit(bench.world, "eat", 10.5, 64.5, 8.5, bench.now);
        bench.advance(4);

        CHECK(bench.changes.size() == 1);
    }

    SUBCASE("anything that is not wool lets it through") {
        Bench bench;
        bench.world.getOrGenerateChunk(0, 0);
        REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));
        // parity: the tag is `#minecraft:wool` and nothing else, so a wall of stone hides nothing.
        REQUIRE(bench.world.setBlockState(9, 64, 8, stateOf("stone", {})));

        vibrations::emit(bench.world, "eat", 10.5, 64.5, 8.5, bench.now);
        bench.advance(4);

        CHECK(bench.changes.size() == 1);
    }
}

TEST_CASE("a calibrated sensor listens the same way") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    const std::int32_t calibrated =
        stateOf("calibrated_sculk_sensor",
                {{"sculk_sensor_phase", "inactive"}, {"power", "0"}, {"waterlogged", "false"}, {"facing", "north"}});
    REQUIRE(bench.world.setBlockState(8, 64, 8, calibrated));

    vibrations::emit(bench.world, "step", 8.5, 64.5, 8.5, bench.now);
    bench.advance();

    REQUIRE(bench.changes.size() == 1);
    CHECK(propertyOf(bench.changes[0].stateId, "sculk_sensor_phase") == "active");
    // Right on top of it, so full strength whatever the noise was.
    CHECK(propertyOf(bench.changes[0].stateId, "power") == "15");
    // The face it was placed on is not touched.
    CHECK(propertyOf(bench.changes[0].stateId, "facing") == "north");
    CHECK(bench.world.hasScheduledBlockTick(8, 64, 8,
                                            basalt::wire::common::Identifier::vanilla("calibrated_sculk_sensor")));
}

TEST_CASE("a calibrated sensor with a signal behind it hears one frequency and no other") {
    // parity: `CalibratedSculkSensorBlockEntity.VibrationUser.canReceiveVibration`, which reads the
    // signal on the square **behind** the sensor and refuses every noise whose frequency is not that
    // number. A zero back hears everything, which is what the plain sensor does.
    const auto sensorFacing = [](const std::string& facing) {
        return stateOf(
            "calibrated_sculk_sensor",
            {{"sculk_sensor_phase", "inactive"}, {"power", "0"}, {"waterlogged", "false"}, {"facing", facing}});
    };
    {
        Bench bench;
        bench.world.getOrGenerateChunk(0, 0);
        REQUIRE(bench.world.setBlockState(8, 64, 8, sensorFacing("north")));
        // Facing north, so the back is south: a block of redstone there calls fifteen.
        REQUIRE(bench.world.setBlockState(8, 64, 9, stateOf("redstone_block", {})));

        // A step is eleven, and eleven is not fifteen.
        vibrations::emit(bench.world, "step", 8.5, 64.5, 8.5, bench.now);
        bench.advance();
        CHECK(bench.changes.empty());
    }
    {
        Bench bench;
        bench.world.getOrGenerateChunk(0, 0);
        REQUIRE(bench.world.setBlockState(8, 64, 8, sensorFacing("north")));
        REQUIRE(bench.world.setBlockState(8, 64, 9, stateOf("redstone_block", {})));

        // `entity_die` is fifteen, which is what this sensor is tuned to.
        REQUIRE(vibrations::frequencyOf("entity_die") == 15);
        vibrations::emit(bench.world, "entity_die", 8.5, 64.5, 8.5, bench.now);
        bench.advance();
        REQUIRE(bench.changes.size() == 1);
        CHECK(propertyOf(bench.changes[0].stateId, "sculk_sensor_phase") == "active");
    }
}

TEST_CASE("an awake sensor settles through cooldown and then goes quiet") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));
    vibrations::emit(bench.world, "eat", 8.5, 64.5, 8.5, bench.now);
    bench.advance();
    REQUIRE(bench.changes.size() == 1);

    // The thirty ticks it stays awake.
    bench.advance(29);
    CHECK(bench.changes.size() == 1);
    bench.advance();
    REQUIRE(bench.changes.size() == 2);
    CHECK(propertyOf(bench.changes[1].stateId, "sculk_sensor_phase") == "cooldown");
    CHECK(propertyOf(bench.changes[1].stateId, "power") == "0");

    // And the ten it takes to settle.
    bench.advance(9);
    CHECK(bench.changes.size() == 2);
    bench.advance();
    REQUIRE(bench.changes.size() == 3);
    CHECK(propertyOf(bench.changes[2].stateId, "sculk_sensor_phase") == "inactive");
    CHECK(bench.world.blockState(8, 64, 8) == sleepingSensor());

    // Once quiet there is nothing left to book or to change.
    CHECK_FALSE(vibrations::tick(bench.world, 8, 64, 8, bench.now).has_value());
    bench.advance(40);
    CHECK(bench.changes.size() == 3);
}

TEST_CASE("a scheduled turn on anything but a sensor does nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::named("stone")->stateId));

    CHECK_FALSE(vibrations::tick(world, 8, 64, 8, 1).has_value());
    CHECK_FALSE(vibrations::tick(world, 8, 400, 8, 1).has_value());
}

TEST_CASE("the source block is the one the position falls in, negative or not") {
    Bench bench;
    bench.world.getOrGenerateChunk(-1, -1);
    REQUIRE(bench.world.setBlockState(-9, 64, -9, sleepingSensor()));

    // Truncating instead of flooring would put the source at -0.5 into block zero, eight away.
    vibrations::emit(bench.world, "eat", -0.5, 64.5, -0.5, bench.now);
    bench.advance(14);

    CHECK(bench.changes.empty());
}

TEST_CASE("a step taken on the sensor gets in while one is already travelling, and waits its turn") {
    // parity: `SculkSensorBlock.stepOn` goes through `forceScheduleVibration`, which adds the
    // candidate straight away: no wool check and no "one already on the way" refusal. What it does
    // **not** do is jump the queue: the selector is only read once the sensor is carrying nothing.
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));

    // Something loud six squares away is already travelling towards it.
    vibrations::emit(bench.world, "explode", 14.5, 64.5, 8.5, bench.now);
    bench.advance();
    REQUIRE(bench.changes.empty());

    // A step taken on the sensor itself is taken in even so, and sits in the selector.
    CHECK(vibrations::stoodOn(bench.world, 8, 64, 8, 8.5, 65.0, 8.5, bench.now));

    bench.advance(5);
    REQUIRE(bench.changes.size() == 1);
    CHECK(bench.frequencyAt(8, 64, 8) == 15);

    // parity: the block entity keeps ticking while the sensor is awake, so the step is picked, it
    // travels, it lands and `onReceiveVibration` throws it away because `canActivate` says no. What
    // it must not do is wait forty ticks and ring the sensor a second time.
    bench.advance(20);
    CHECK(bench.changes.size() == 1);
    CHECK(bench.frequencyAt(8, 64, 8) == 15);
}

TEST_CASE("a sensor already awake takes no step from being stood on") {
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(8, 64, 8, sleepingSensor()));
    vibrations::emit(bench.world, "eat", 8.5, 64.5, 8.5, bench.now);
    bench.advance();
    REQUIRE(bench.changes.size() == 1);

    // parity: `canActivate` still gates it, so an awake sensor is not stepped on into ringing again.
    CHECK_FALSE(vibrations::stoodOn(bench.world, 8, 64, 8, 8.5, 65.0, 8.5, bench.now));
}

TEST_CASE("an amethyst block beside a sensor carries the reading on to the next one") {
    // parity: `SculkSensorBlock.tryResonateVibration`, which sends the same frequency out again from
    // the amethyst's own square. It is how a sculk farm reaches past the eight squares a sensor
    // hears, and none of it was here.
    Bench bench;
    bench.world.getOrGenerateChunk(0, 0);
    REQUIRE(bench.world.setBlockState(2, 64, 8, sleepingSensor()));
    REQUIRE(bench.world.setBlockState(3, 64, 8, stateOf("amethyst_block", {})));
    // Far enough from the first noise that only the amethyst can reach it.
    REQUIRE(bench.world.setBlockState(9, 64, 8, sleepingSensor()));

    vibrations::emit(bench.world, "eat", 2.5, 64.5, 8.5, bench.now);
    bench.advance();
    REQUIRE(bench.changes.size() == 1);
    CHECK(bench.frequencyAt(2, 64, 8) == 8);

    // The amethyst repeats it as `resonate_8`, six squares from the second sensor.
    bench.advance(6);
    CHECK(propertyOf(*bench.world.blockState(9, 64, 8), "sculk_sensor_phase") == "active");
    CHECK(bench.frequencyAt(9, 64, 8) == 8);
}

TEST_CASE("a resonance event is worth the number in its name") {
    // parity: the tail of `VIBRATION_FREQUENCY_FOR_EVENT`, fifteen entries put in by a loop.
    CHECK(vibrations::frequencyOf("resonate_1") == 1);
    CHECK(vibrations::frequencyOf("resonate_8") == 8);
    CHECK(vibrations::frequencyOf("resonate_15") == 15);
    CHECK(vibrations::frequencyOf("resonate_0") == 0);
    CHECK(vibrations::frequencyOf("resonate_16") == 0);
    CHECK(vibrations::frequencyOf("resonate_x") == 0);
    CHECK(vibrations::frequencyOf("resonate_") == 0);
}

TEST_CASE("a chunk that arrives with a sensor in it is known before anybody touches it") {
    // The tracker is what a footstep asks before it looks around, so a sensor it does not know about
    // is a sensor that hears nobody walk. Until this, only sensors placed or broken at runtime were
    // in it: one that came with the terrain counted for nothing.
    WorldState world{{}, [](std::int32_t chunkX, std::int32_t chunkZ) {
                         Chunk chunk{chunkX, chunkZ};
                         if (chunkX == 0 && chunkZ == 0) {
                             chunk.setBlockEntity(basalt::server::world::ChunkBlockEntity{
                                 8, 64, 8, basalt::wire::common::Identifier::vanilla("sculk_sensor"),
                                 basalt::wire::nbt::NbtCompound{}});
                         }
                         return chunk;
                     }};
    CHECK_FALSE(world.hasSculkSensorNear(8.5, 64.5, 8.5, vibrations::kRange));

    world.getOrGenerateChunk(0, 0);

    CHECK(world.hasSculkSensorNear(8.5, 64.5, 8.5, vibrations::kRange));
    // A sensor sixteen squares away is out of the eight the listener reaches.
    CHECK_FALSE(world.hasSculkSensorNear(30.5, 64.5, 8.5, vibrations::kRange));
}

TEST_CASE("a sensor the terrain wrote counts too, block entity or not") {
    // The deep dark's sensors are written straight into the section by the generator and carry no
    // block entity, so reading only the block entity list missed every one of them. The palette says
    // cheaply which sections can hold one, and only those are walked.
    WorldState world{{}, [](std::int32_t chunkX, std::int32_t chunkZ) {
                         Chunk chunk{chunkX, chunkZ};
                         if (chunkX == 0 && chunkZ == 0) {
                             chunk.setBlockStateId(8, 64, 8, sleepingSensor());
                         }
                         return chunk;
                     }};

    world.getOrGenerateChunk(0, 0);

    CHECK(world.hasSculkSensorNear(8.5, 64.5, 8.5, vibrations::kRange));
    CHECK_FALSE(world.hasSculkSensorNear(30.5, 64.5, 8.5, vibrations::kRange));
}

TEST_CASE("a sensor's click reaches a shrieker only when a player made the noise") {
    // parity: `SculkSensorBlock.activate` raises `sculk_sensor_tendrils_clicking` with the first
    // noise's source, and `SculkShriekerBlockEntity.VibrationUser` hears it only through a player.
    const basalt::wire::common::Uuid player{1, 2};
    const std::int32_t quietShrieker =
        stateOf("sculk_shrieker", {{"shrieking", "false"}, {"can_summon", "false"}, {"waterlogged", "false"}});
    for (const bool byPlayer : {true, false}) {
        CAPTURE(byPlayer);
        Bench bench;
        // parity: a shrieker only takes a noise while the three by three chunks around it run.
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                bench.world.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
        bench.world.setPlayerCheck([player](const basalt::wire::common::Uuid& entity) { return entity == player; });
        REQUIRE(bench.world.setBlockState(2, 64, 8, sleepingSensor()));
        REQUIRE(bench.world.setBlockState(6, 64, 8, quietShrieker));

        const vibrations::Source source = byPlayer ? vibrations::Source{.entity = player}
                                                   : vibrations::Source{.entity = basalt::wire::common::Uuid{7, 7}};
        vibrations::emit(bench.world, "eat", 2.5, 64.5, 8.5, bench.now, source);
        std::vector<vibrations::ShriekerArrival> arrivals;
        for (std::int32_t turn = 0; turn < 8; ++turn) {
            bench.advance();
            if (const std::optional<vibrations::ShriekerArrival> arrival =
                    vibrations::shriekerTravelTick(bench.world, 6, 64, 8, bench.now);
                arrival.has_value()) {
                arrivals.push_back(*arrival);
            }
        }
        REQUIRE(propertyOf(*bench.world.blockState(2, 64, 8), "sculk_sensor_phase") != "inactive");
        if (byPlayer) {
            REQUIRE(arrivals.size() == 1);
            CHECK(arrivals.front().source.entity == player);
        } else {
            CHECK(arrivals.empty());
        }
    }
}
