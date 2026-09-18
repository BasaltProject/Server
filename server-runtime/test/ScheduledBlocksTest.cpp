#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

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

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A world with the middle chunk in memory, since `WorldState` cannot be moved out of a helper.
struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

/// How many ticks from now the block at these coordinates was booked for, or -1 if it was not.
[[nodiscard]] std::int32_t bookedIn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                    std::int32_t limit) {
    for (std::int32_t turn = 0; turn < limit; ++turn) {
        // parity: the turn opens before anything runs, and that is where the countdown happens.
        world.openTurn();
        for (const std::array<std::int32_t, 3>& position : world.dueBlockTicks()) {
            if (position[0] == x && position[1] == y && position[2] == z) {
                return turn + 1;
            }
        }
    }
    return -1;
}

} // namespace

TEST_CASE("soil under something solid gives way to dirt") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));

    CHECK(scheduledBlocks::coveredSoil(world, 8, 64, 8) == stateOf("dirt"));
    CHECK(world.blockState(8, 64, 8) == stateOf("dirt"));

    REQUIRE(world.setBlockState(4, 64, 4, stateOf("dirt_path")));
    REQUIRE(world.setBlockState(4, 65, 4, stateOf("stone")));
    CHECK(scheduledBlocks::coveredSoil(world, 4, 64, 4) == stateOf("dirt"));
}

TEST_CASE("what is not solid is not a lid") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t farmland = stateOf("farmland", {{"moisture", "7"}});
    REQUIRE(world.setBlockState(8, 64, 8, farmland));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "3"}})));

    CHECK_FALSE(scheduledBlocks::coveredSoil(world, 8, 64, 8).has_value());
    CHECK(world.blockState(8, 64, 8) == farmland);

    // Open sky above it is not something solid either.
    REQUIRE(world.setBlockState(8, 65, 8, air()));
    CHECK_FALSE(scheduledBlocks::coveredSoil(world, 8, 64, 8).has_value());
    // And nothing else is soil.
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("stone")));
    REQUIRE(world.setBlockState(4, 65, 4, stateOf("stone")));
    CHECK_FALSE(scheduledBlocks::coveredSoil(world, 4, 64, 4).has_value());
}

TEST_CASE("coral out of the water dies where it stands") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("tube_coral_block")));

    CHECK(scheduledBlocks::coral(world, 8, 64, 8) == stateOf("dead_tube_coral_block"));

    // Already dead is as dead as it gets.
    CHECK_FALSE(scheduledBlocks::coral(world, 8, 64, 8).has_value());
}

TEST_CASE("coral with water anywhere around it lives") {
    Loaded loaded;
    WorldState& world = loaded.state;
    for (const std::array<std::int32_t, 3>& side :
         {std::array<std::int32_t, 3>{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}) {
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("tube_coral_block")));
        REQUIRE(world.setBlockState(8 + side[0], 64 + side[1], 8 + side[2], stateOf("water")));
        CHECK_FALSE(scheduledBlocks::coral(world, 8, 64, 8).has_value());
        REQUIRE(world.setBlockState(8 + side[0], 64 + side[1], 8 + side[2], air()));
    }

    // A waterlogged fan carries its own water.
    const std::int32_t fan = stateOf("tube_coral_fan", {{"waterlogged", "true"}});
    REQUIRE(world.setBlockState(4, 64, 4, fan));
    CHECK_FALSE(scheduledBlocks::coral(world, 4, 64, 4).has_value());
}

TEST_CASE("scaffolding counts its way out from what holds it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto scaffold = [](std::int32_t distance, const std::string& bottom) {
        return stateOf("scaffolding",
                       {{"distance", std::to_string(distance)}, {"bottom", bottom}, {"waterlogged", "false"}});
    };
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));

    // Standing on the ground it is zero away, and not a bottom piece.
    REQUIRE(world.setBlockState(8, 64, 8, scaffold(7, "true")));
    CHECK(scheduledBlocks::scaffolding(world, 8, 64, 8) == scaffold(0, "false"));

    // Each one out to the side is one further, up to seven.
    for (std::int32_t step = 1; step <= 7; ++step) {
        REQUIRE(world.setBlockState(8 + step, 64, 8, scaffold(7, "true")));
        const std::optional<std::int32_t> counted = scheduledBlocks::scaffolding(world, 8 + step, 64, 8);
        if (step < 7) {
            REQUIRE(counted.has_value());
            CHECK(propertyOf(*counted, "distance") == std::to_string(step));
            CHECK(propertyOf(*counted, "bottom") == "true");
            CHECK_FALSE(scheduledBlocks::scaffoldingHasFallen(world, 8 + step, 64, 8));
        } else {
            // Seven out is out of reach, which is the state it was already in.
            CHECK_FALSE(counted.has_value());
            CHECK(scheduledBlocks::scaffoldingHasFallen(world, 8 + step, 64, 8));
        }
    }
}

TEST_CASE("scaffolding stacked on scaffolding keeps the distance of the piece below") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t loose =
        stateOf("scaffolding", {{"distance", "7"}, {"bottom", "true"}, {"waterlogged", "false"}});
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, loose));
    REQUIRE(scheduledBlocks::scaffolding(world, 8, 64, 8).has_value());
    REQUIRE(world.setBlockState(8, 65, 8, loose));

    const std::optional<std::int32_t> stacked = scheduledBlocks::scaffolding(world, 8, 65, 8);
    REQUIRE(stacked.has_value());
    CHECK(propertyOf(*stacked, "distance") == "0");
    CHECK(propertyOf(*stacked, "bottom") == "false");
}

TEST_CASE("frosted ice melts at eleven and waits at ten") {
    // A torch three blocks away puts exactly eleven on the ice, which is the light that melts it
    // once its own dampening of one is taken off the limit.
    const auto meltsWith = [](std::int32_t distance, std::int32_t age) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("frosted_ice", {{"age", std::to_string(age)}})));
        REQUIRE(world.setBlockState(8 + distance, 64, 8, stateOf("torch")));
        rng::RandomSource random = rng::RandomSource::legacy(1);
        std::vector<BlockChange> changes;
        scheduledBlocks::frostedIce(world, 8, 64, 8, random, changes, false);
        return !changes.empty();
    };

    // A torch that many blocks away puts fourteen minus that on the ice, and every step of age
    // lowers the light it will still wait at by one.
    for (std::int32_t age = 0; age <= 3; ++age) {
        CHECK(meltsWith(3 + age, age));
        CHECK_FALSE(meltsWith(4 + age, age));
    }
}

TEST_CASE("frosted ice packed in with its own kind mostly waits") {
    const auto meltsBeside = [](std::int32_t neighbours, std::int64_t seed) {
        Loaded loaded;
        WorldState& world = loaded.state;
        const std::int32_t ice = stateOf("frosted_ice", {{"age", "0"}});
        REQUIRE(world.setBlockState(8, 64, 8, ice));
        const std::array<std::array<std::int32_t, 3>, 5> sides{
            {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}}};
        for (std::int32_t index = 0; index < neighbours; ++index) {
            REQUIRE(world.setBlockState(8 + sides[static_cast<std::size_t>(index)][0],
                                        64 + sides[static_cast<std::size_t>(index)][1],
                                        8 + sides[static_cast<std::size_t>(index)][2], ice));
        }
        rng::RandomSource random = rng::RandomSource::legacy(seed);
        std::vector<BlockChange> changes;
        scheduledBlocks::frostedIce(world, 8, 64, 8, random, changes);
        return !changes.empty();
    };

    // Seed two rolls something other than zero on the first draw of three, which is the roll that
    // lets a crowded block wait.
    CHECK_FALSE(meltsBeside(4, 2));
    CHECK(meltsBeside(3, 2));
    // One roll in three melts it anyway, however crowded it is.
    CHECK(meltsBeside(4, 1));
}

TEST_CASE("ice that goes whole thins its neighbours on the spot") {
    // parity: `FrostedIceBlock.tick` reaches the six squares **only** when `slightlyMelt` answered
    // true, which it does only for the last age, and it calls `slightlyMelt` on each neighbour rather
    // than just booking it. The neighbour that survives that is the one that gets a turn.
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto ice = [](std::int32_t age) { return stateOf("frosted_ice", {{"age", std::to_string(age)}}); };
    REQUIRE(world.setBlockState(8, 64, 8, ice(3)));
    REQUIRE(world.setBlockState(9, 64, 8, ice(0)));
    REQUIRE(world.setBlockState(7, 64, 8, ice(3)));
    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;

    scheduledBlocks::frostedIce(world, 8, 64, 8, random, changes);

    CHECK(world.blockState(8, 64, 8) == stateOf("water"));
    // The young one thinned and kept its place, so it books a turn between twenty and forty.
    CHECK(world.blockState(9, 64, 8) == ice(1));
    CHECK(world.hasScheduledBlockTick(9, 64, 8, scheduledBlocks::frostedIceTickType()));
    // The old one went with it, and water books nothing.
    CHECK(world.blockState(7, 64, 8) == stateOf("water"));
    CHECK_FALSE(world.hasScheduledBlockTick(7, 64, 8, scheduledBlocks::frostedIceTickType()));
    // parity: a piece that only thins never reaches its neighbours at all.
    Loaded young;
    REQUIRE(young.state.setBlockState(8, 64, 8, ice(0)));
    REQUIRE(young.state.setBlockState(9, 64, 8, ice(0)));
    std::vector<BlockChange> thinned;
    scheduledBlocks::frostedIce(young.state, 8, 64, 8, random, thinned);
    CHECK(young.state.blockState(9, 64, 8) == ice(0));
    CHECK_FALSE(young.state.hasScheduledBlockTick(9, 64, 8, scheduledBlocks::frostedIceTickType()));
    CHECK(young.state.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::frostedIceTickType()));
}

TEST_CASE("frosted ice thins in the light and asks again in the dark") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto ice = [](std::int32_t age) { return stateOf("frosted_ice", {{"age", std::to_string(age)}}); };
    REQUIRE(world.setBlockState(8, 64, 8, ice(0)));
    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;

    scheduledBlocks::frostedIce(world, 8, 64, 8, random, changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].stateId == ice(1));
    // One step at a time: it is only the last age that turns to water.
    REQUIRE(world.setBlockState(4, 64, 4, ice(2)));
    scheduledBlocks::frostedIce(world, 4, 64, 4, random, changes);
    CHECK(world.blockState(4, 64, 4) == ice(3));
    CHECK(world.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::frostedIceTickType()));

    // Under a roof there is not enough light, so it only books another turn.
    Loaded dark;
    WorldState& night = dark.state;
    REQUIRE(night.setBlockState(8, 64, 8, ice(0)));
    REQUIRE(night.setBlockState(8, 65, 8, stateOf("stone")));
    std::vector<BlockChange> nightChanges;
    scheduledBlocks::frostedIce(night, 8, 64, 8, random, nightChanges, false);
    CHECK(nightChanges.empty());
    CHECK(night.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::frostedIceTickType()));
}

TEST_CASE("the last of the frosted ice is water") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("frosted_ice", {{"age", "3"}})));
    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;

    scheduledBlocks::frostedIce(world, 8, 64, 8, random, changes);

    REQUIRE(changes.size() == 1);
    CHECK(changes[0].stateId == stateOf("water"));
    CHECK(world.blockState(8, 64, 8) == stateOf("water"));
}

TEST_CASE("a dried ghast fills up in water and dries out of it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto ghast = [](std::int32_t hydration, const std::string& waterlogged) {
        return stateOf("dried_ghast",
                       {{"hydration", std::to_string(hydration)}, {"waterlogged", waterlogged}, {"facing", "north"}});
    };
    std::vector<BlockChange> changes;
    std::vector<scheduledBlocks::Hatchling> hatched;

    REQUIRE(world.setBlockState(8, 64, 8, ghast(0, "true")));
    scheduledBlocks::driedGhast(world, 8, 64, 8, 0, changes, hatched);
    CHECK(world.blockState(8, 64, 8) == ghast(1, "true"));
    // parity: `DriedGhastBlock.tick` books nothing at all. The next step of the clock comes from a
    // random tick, so a block that armed its own turn here would run faster than the jar's.
    CHECK_FALSE(world.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::driedGhastTickType()));

    REQUIRE(world.setBlockState(4, 64, 4, ghast(2, "false")));
    scheduledBlocks::driedGhast(world, 4, 64, 4, 0, changes, hatched);
    CHECK(world.blockState(4, 64, 4) == ghast(1, "false"));

    // Two steps in and still soaking, it keeps filling to the top.
    REQUIRE(world.setBlockState(7, 64, 7, ghast(2, "true")));
    scheduledBlocks::driedGhast(world, 7, 64, 7, 0, changes, hatched);
    CHECK(world.blockState(7, 64, 7) == ghast(3, "true"));

    // Empty out of water is the end of the road, and full in it is a ghastling.
    REQUIRE(world.setBlockState(6, 64, 6, ghast(0, "false")));
    const std::size_t before = changes.size();
    scheduledBlocks::driedGhast(world, 6, 64, 6, 0, changes, hatched);
    CHECK(changes.size() == before);
    CHECK(hatched.empty());

    REQUIRE(world.setBlockState(5, 64, 5, ghast(3, "true")));
    scheduledBlocks::driedGhast(world, 5, 64, 5, 0, changes, hatched);
    CHECK(world.blockState(5, 64, 5) == air());
    REQUIRE(hatched.size() == 1);
    CHECK(hatched.front().type.path() == "happy_ghast");
    CHECK(hatched.front().baby);
    // parity: `Vec3.atBottomCenterOf`, and the yaw the block was facing.
    CHECK(hatched.front().x == doctest::Approx(5.5));
    CHECK(hatched.front().y == doctest::Approx(64.0));
    CHECK(hatched.front().z == doctest::Approx(5.5));
    CHECK(hatched.front().yaw == doctest::Approx(180.0F));
}

TEST_CASE("a sniffer egg hatches sooner on moss") {
    const auto booked = [](const std::string& below) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 63, 8, stateOf(below)));
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("sniffer_egg", {{"hatch", "0"}})));
        scheduledBlocks::scheduleHatch(world, 8, 64, 8, 7);
        return bookedIn(world, 8, 64, 8, 20000);
    };

    // A third of the hatch each time, plus the same spread from the same seed.
    const std::int32_t plain = booked("stone");
    const std::int32_t moss = booked("moss_block");
    CHECK(plain == 8136);
    CHECK(moss == 4136);
}

TEST_CASE("frogspawn is booked for an hour or so and then goes") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("frogspawn")));

    scheduledBlocks::scheduleFrogspawn(world, 8, 64, 8, 3);
    CHECK(bookedIn(world, 8, 64, 8, 13000) == 10934);

    std::vector<BlockChange> changes;
    std::vector<scheduledBlocks::Hatchling> hatched;
    rng::RandomSource random = rng::RandomSource::legacy(9);
    scheduledBlocks::frogspawn(world, 8, 64, 8, random, changes, hatched);
    REQUIRE(changes.size() == 1);
    CHECK(world.blockState(8, 64, 8) == air());
    // parity: `spawnTadpoles`, two to five of them inside the square and half a block down.
    CHECK(hatched.size() >= 2);
    CHECK(hatched.size() <= 5);
    for (const scheduledBlocks::Hatchling& tadpole : hatched) {
        CHECK(tadpole.type.path() == "tadpole");
        CHECK(tadpole.persistent);
        CHECK(tadpole.y == doctest::Approx(63.5));
        CHECK(tadpole.x >= 8.2);
        CHECK(tadpole.x <= 8.8);
        CHECK(tadpole.z >= 8.2);
        CHECK(tadpole.z <= 8.8);
    }
}

TEST_CASE("a sniffer egg cracks twice and then hatches a calf") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sniffer_egg", {{"hatch", "0"}})));
    rng::RandomSource random = rng::RandomSource::legacy(5);
    std::vector<BlockChange> changes;
    std::vector<scheduledBlocks::Hatchling> hatched;

    scheduledBlocks::snifferEgg(world, 8, 64, 8, random, changes, hatched);
    CHECK(propertyOf(*world.blockState(8, 64, 8), "hatch") == "1");
    scheduledBlocks::snifferEgg(world, 8, 64, 8, random, changes, hatched);
    CHECK(propertyOf(*world.blockState(8, 64, 8), "hatch") == "2");
    CHECK(hatched.empty());
    scheduledBlocks::snifferEgg(world, 8, 64, 8, random, changes, hatched);
    CHECK(world.blockState(8, 64, 8) == air());
    CHECK(changes.size() == 3);
    // parity: `Vec3.atCenterOf`, so the calf starts half a block up inside the square it hatched in.
    REQUIRE(hatched.size() == 1);
    CHECK(hatched.front().type.path() == "sniffer");
    CHECK(hatched.front().baby);
    CHECK(hatched.front().x == doctest::Approx(8.5));
    CHECK(hatched.front().y == doctest::Approx(64.5));
    CHECK(hatched.front().z == doctest::Approx(8.5));
}

TEST_CASE("soul sand pushes a column up and magma drags one down") {
    const auto columnOver = [](const std::string& below) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 63, 8, stateOf(below)));
        for (std::int32_t y = 64; y < 68; ++y) {
            REQUIRE(world.setBlockState(8, y, 8, stateOf("water")));
        }
        std::vector<BlockChange> changes;
        scheduledBlocks::bubbleColumn(world, 8, 64, 8, changes);
        return changes;
    };

    const std::vector<BlockChange> rising = columnOver("soul_sand");
    CHECK(rising.size() == 4);
    CHECK(pathOf(rising[0].stateId) == "bubble_column");
    CHECK(propertyOf(rising[0].stateId, "drag") == "false");
    CHECK(rising[3].y == 67);

    const std::vector<BlockChange> sinking = columnOver("magma_block");
    CHECK(sinking.size() == 4);
    CHECK(propertyOf(sinking[0].stateId, "drag") == "true");
}

TEST_CASE("a column with nothing feeding it goes back to water") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t column = stateOf("bubble_column", {{"drag", "false"}});
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));
    for (std::int32_t y = 64; y < 67; ++y) {
        REQUIRE(world.setBlockState(8, y, 8, column));
    }
    std::vector<BlockChange> changes;

    scheduledBlocks::bubbleColumn(world, 8, 64, 8, changes);

    CHECK(changes.size() == 3);
    for (std::int32_t y = 64; y < 67; ++y) {
        CHECK(world.blockState(8, y, 8) == stateOf("water"));
    }
}

TEST_CASE("a column already built does not reach past itself to the water on top") {
    // parity: `updateColumn` throws away the answer of its **first** write and then stops at the
    // first one that changes nothing, because `Level.setBlock` says false when the state does not
    // move. So a column that is already there ends the walk at its own second square, and the water
    // sitting on top has to wait for its own notice. No bench circuit can reach this: everything
    // that gives the bottom square a turn also changes the state it is about to write.
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t column = stateOf("bubble_column", {{"drag", "false"}});
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("soul_sand")));
    for (std::int32_t y = 64; y < 67; ++y) {
        REQUIRE(world.setBlockState(8, y, 8, column));
    }
    REQUIRE(world.setBlockState(8, 67, 8, stateOf("water")));
    std::vector<BlockChange> changes;

    scheduledBlocks::bubbleColumn(world, 8, 64, 8, changes);

    CHECK(changes.empty());
    CHECK(world.blockState(8, 67, 8) == stateOf("water"));
}

TEST_CASE("the water under a column carries itself up and washes the column away") {
    // parity: the last branch of `getColumnState` is `occupyState`, the square's own state. When the
    // turn belongs to `LiquidBlock.tick` and nothing feeds the column any more, what walks upwards is
    // the water, and it turns whatever is over it back into water.
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t column = stateOf("bubble_column", {{"drag", "false"}});
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("water")));
    REQUIRE(world.setBlockState(8, 65, 8, column));
    REQUIRE(world.setBlockState(8, 66, 8, column));
    std::vector<BlockChange> changes;

    scheduledBlocks::bubbleColumn(world, 8, 64, 8, changes);

    CHECK(changes.size() == 2);
    CHECK(world.blockState(8, 65, 8) == stateOf("water"));
    CHECK(world.blockState(8, 66, 8) == stateOf("water"));
}

TEST_CASE("a column stops where the water does") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("soul_sand")));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("water")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("water")));
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("stone")));
    std::vector<BlockChange> changes;

    scheduledBlocks::bubbleColumn(world, 8, 64, 8, changes);

    CHECK(changes.size() == 2);
    CHECK(world.blockState(8, 66, 8) == stateOf("stone"));
}

TEST_CASE("a creaking heart only has a night in the overworld") {
    // parity: `creaking_active` is a track of the `minecraft:day` timeline, tagged
    // `#minecraft:in_overworld` and nowhere else, so the attribute keeps its own `false` in the
    // Nether and the End whatever their clock says.
    CHECK_FALSE(scheduledBlocks::creakingIsActive(12599));
    CHECK(scheduledBlocks::creakingIsActive(12600));
    CHECK(scheduledBlocks::creakingIsActive(23400));
    CHECK_FALSE(scheduledBlocks::creakingIsActive(23401));
    for (const std::int64_t ticks : {0, 12600, 18000, 23400}) {
        CHECK_FALSE(scheduledBlocks::creakingIsActive(ticks, false));
    }
}
