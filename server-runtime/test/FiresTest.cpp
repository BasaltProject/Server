#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Fires.hpp>

#include <doctest.h>

#include <array>
#include <map>
#include <string>
#include <utility>
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

[[nodiscard]] std::int32_t fireAged(std::int32_t age) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("fire"), {{"age", std::to_string(age)}}, std::nullopt);
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

/// A stone floor at y=63 across the middle chunk.
void floor(WorldState& world, const std::string& block = "stone") {
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 63, z, stateOf(block)));
        }
    }
}

[[nodiscard]] std::int32_t firesAround(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    std::int32_t found = 0;
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dy = -1; dy <= 5; ++dy) {
            for (std::int32_t dz = -2; dz <= 2; ++dz) {
                const std::optional<std::int32_t> stateId = world.blockState(x + dx, y + dy, z + dz);
                if (stateId.has_value() && fires::isFire(*stateId)) {
                    ++found;
                }
            }
        }
    }
    return found;
}

const WorldWeather kDry{};

[[nodiscard]] WorldWeather rain() {
    WorldWeather weather;
    weather.raining = true;
    return weather;
}

/// A fire that cannot die (netherrack under it) with a single plank as the only thing around that
/// will catch, so what lights up says exactly which offsets `spread` reaches and how often.
///
/// Answers the turn on which the candidate first caught, or -1 if it never did.
[[nodiscard]] std::int32_t firstCatch(std::array<std::int32_t, 3> plank, std::array<std::int32_t, 3> candidate,
                                      const std::string& difficulty = "hard", std::int32_t turns = 400) {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("netherrack")));
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    REQUIRE(world.setBlockState(plank[0], plank[1], plank[2], stateOf("oak_planks")));
    rng::RandomSource random = rng::RandomSource::legacy(19);
    for (std::int32_t turn = 0; turn < turns; ++turn) {
        static_cast<void>(fires::tick(world, kDry, difficulty, 8, 64, 8, random));
        const std::optional<std::int32_t> stateId = world.blockState(candidate[0], candidate[1], candidate[2]);
        if (stateId.has_value() && fires::isFire(*stateId)) {
            return turn;
        }
    }
    return -1;
}

} // namespace

TEST_CASE("a fire acts again between thirty and thirty-nine ticks later") {
    rng::RandomSource random = rng::RandomSource::legacy(7);
    std::int32_t lowest = fires::kMinDelay + fires::kDelaySpread;
    std::int32_t highest = 0;
    for (std::int32_t roll = 0; roll < 2000; ++roll) {
        const std::int32_t delay = fires::tickDelay(random);
        CHECK(delay >= fires::kMinDelay);
        CHECK(delay < fires::kMinDelay + fires::kDelaySpread);
        lowest = std::min(lowest, delay);
        highest = std::max(highest, delay);
    }
    CHECK(lowest == 30);
    CHECK(highest == 39);
}

TEST_CASE("only fire counts as fire") {
    CHECK(fires::isFire(fireAged(0)));
    CHECK(fires::isFire(fireAged(15)));
    CHECK_FALSE(fires::isFire(stateOf("soul_fire")));
    CHECK_FALSE(fires::isFire(registry::blocks::air().stateId));
}

TEST_CASE("flint and steel lights an empty space and arms its first tick") {
    WorldState world = emptyWorld();
    floor(world);
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(fires::ignite(world, 8, 64, 8, random));
    CHECK(world.blockState(8, 64, 8) == fireAged(0));
    CHECK(world.hasScheduledBlockTick(8, 64, 8, fires::tickType()));
}

TEST_CASE("nothing is lit where a block already stands") {
    WorldState world = emptyWorld();
    floor(world);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK_FALSE(fires::ignite(world, 8, 64, 8, random));
    CHECK(world.blockState(8, 64, 8) == stateOf("stone"));
    CHECK_FALSE(world.hasScheduledBlockTick(8, 64, 8, fires::tickType()));
}

TEST_CASE("a turn on anything that is not fire does nothing") {
    WorldState world = emptyWorld();
    floor(world);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("oak_planks")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("soul_fire")));
    rng::RandomSource random = rng::RandomSource::legacy(3);

    CHECK(fires::tick(world, kDry, "normal", 8, 64, 8, random).empty());
    CHECK(fires::tick(world, kDry, "normal", 8, 65, 8, random).empty());
    CHECK(fires::tick(world, kDry, "normal", 8, 400, 8, random).empty());
    CHECK(world.blockState(8, 64, 8) == stateOf("oak_planks"));
    CHECK(world.blockState(8, 65, 8) == stateOf("soul_fire"));
}

TEST_CASE("a fire over stone with nothing to burn ages and then dies") {
    WorldState world = emptyWorld();
    floor(world);
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    rng::RandomSource random = rng::RandomSource::legacy(3);

    std::int32_t turns = 0;
    while (world.blockState(8, 64, 8) != registry::blocks::air().stateId && turns < 200) {
        const std::vector<BlockChange> changes = fires::tick(world, kDry, "normal", 8, 64, 8, random);
        CHECK(changes.size() <= 2);
        ++turns;
    }
    // Ageing is one step every other turn on average, and death waits for age to pass three.
    CHECK(turns == 15);
    CHECK(world.blockState(8, 64, 8) == registry::blocks::air().stateId);
}

TEST_CASE("a fire whose floor was taken away goes out at once") {
    WorldState world = emptyWorld();
    floor(world);
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    REQUIRE(world.setBlockState(8, 63, 8, registry::blocks::air().stateId));
    rng::RandomSource random = rng::RandomSource::legacy(3);

    const std::vector<BlockChange> changes = fires::tick(world, kDry, "normal", 8, 64, 8, random);
    CHECK(changes.size() == 1);
    CHECK(world.blockState(8, 64, 8) == registry::blocks::air().stateId);
}

TEST_CASE("rain on an open sky puts a fire out") {
    WorldState world = emptyWorld();
    floor(world);
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    rng::RandomSource random = rng::RandomSource::legacy(11);

    std::int32_t turns = 0;
    while (world.blockState(8, 64, 8) != registry::blocks::air().stateId && turns < 100) {
        static_cast<void>(fires::tick(world, rain(), "normal", 8, 64, 8, random));
        ++turns;
    }
    // A fresh fire is doused on a roll of one in five, so it never lasts the fifteen turns it takes
    // to die of old age.
    CHECK(turns == 2);
}

TEST_CASE("a roof keeps the rain off") {
    WorldState world = emptyWorld();
    floor(world);
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    REQUIRE(world.setBlockState(8, 70, 8, stateOf("stone")));
    rng::RandomSource random = rng::RandomSource::legacy(11);

    std::int32_t turns = 0;
    while (world.blockState(8, 64, 8) != registry::blocks::air().stateId && turns < 100) {
        static_cast<void>(fires::tick(world, rain(), "normal", 8, 64, 8, random));
        ++turns;
    }
    CHECK(turns == 11);
}

TEST_CASE("fire on netherrack never goes out") {
    WorldState world = emptyWorld();
    floor(world, "netherrack");
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    rng::RandomSource random = rng::RandomSource::legacy(3);

    for (std::int32_t turn = 0; turn < 200; ++turn) {
        static_cast<void>(fires::tick(world, kDry, "normal", 8, 64, 8, random));
    }
    const std::optional<std::int32_t> stateId = world.blockState(8, 64, 8);
    REQUIRE(stateId.has_value());
    CHECK(fires::isFire(*stateId));
}

TEST_CASE("fire eats the planks beside it and spreads through them") {
    WorldState world = emptyWorld();
    floor(world);
    for (std::int32_t x = 4; x < 13; ++x) {
        for (std::int32_t z = 4; z < 13; ++z) {
            REQUIRE(world.setBlockState(x, 64, z, stateOf("oak_planks")));
        }
    }
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    rng::RandomSource random = rng::RandomSource::legacy(5);

    std::int32_t burned = 0;
    for (std::int32_t turn = 0; turn < 60; ++turn) {
        for (const BlockChange& change : fires::tick(world, kDry, "normal", 8, 64, 8, random)) {
            if (change.stateId == registry::blocks::air().stateId || fires::isFire(change.stateId)) {
                ++burned;
            }
        }
    }
    CHECK(burned > 0);
    CHECK(firesAround(world, 8, 64, 8) > 1);
}

TEST_CASE("fire over soul soil burns as soul fire") {
    WorldState world = emptyWorld();
    floor(world, "soul_soil");
    for (std::int32_t x = 6; x < 11; ++x) {
        for (std::int32_t z = 6; z < 11; ++z) {
            REQUIRE(world.setBlockState(x, 64, z, stateOf("oak_planks")));
        }
    }
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    rng::RandomSource random = rng::RandomSource::legacy(5);

    bool sawSoulFire = false;
    for (std::int32_t turn = 0; turn < 60 && !sawSoulFire; ++turn) {
        for (const BlockChange& change : fires::tick(world, kDry, "normal", 8, 64, 8, random)) {
            sawSoulFire = sawSoulFire || change.stateId == stateOf("soul_fire");
        }
    }
    CHECK(sawSoulFire);
}

TEST_CASE("fire reaches one block out, four up and one down, and no further") {
    // The turn each of these catches on pins the odds and the thinning rate as well as the reach.
    CHECK(firstCatch({9, 63, 9}, {9, 64, 9}) == 9);
    CHECK(firstCatch({9, 63, 8}, {9, 64, 8}) == 44);
    CHECK(firstCatch({9, 62, 8}, {9, 63, 8}) == 26);
    CHECK(firstCatch({8, 67, 8}, {8, 68, 8}) == 130);

    CHECK(firstCatch({10, 63, 8}, {10, 64, 8}) == -1);
    CHECK(firstCatch({8, 63, 10}, {8, 64, 10}) == -1);
    CHECK(firstCatch({9, 61, 8}, {9, 62, 8}) == -1);
    CHECK(firstCatch({8, 68, 8}, {8, 69, 8}) == -1);
}

TEST_CASE("nothing catches where there is nothing to burn") {
    WorldState world = emptyWorld();
    floor(world, "netherrack");
    REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
    rng::RandomSource random = rng::RandomSource::legacy(19);

    for (std::int32_t turn = 0; turn < 400; ++turn) {
        // The fire's own ageing is the only thing that ever changes.
        for (const BlockChange& change : fires::tick(world, kDry, "hard", 8, 64, 8, random)) {
            CHECK(change.x == 8);
            CHECK(change.y == 64);
            CHECK(change.z == 8);
        }
    }
    CHECK(firesAround(world, 8, 64, 8) == 1);
}

TEST_CASE("a harder world spreads fire further") {
    const auto spreadsUnder = [](const std::string& difficulty) {
        WorldState world = emptyWorld();
        floor(world, "netherrack");
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 0; z < 16; ++z) {
                REQUIRE(world.setBlockState(x, 65, z, stateOf("oak_planks")));
            }
        }
        REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
        rng::RandomSource random = rng::RandomSource::legacy(2);
        for (std::int32_t turn = 0; turn < 12; ++turn) {
            static_cast<void>(fires::tick(world, kDry, difficulty, 8, 64, 8, random));
        }
        return firesAround(world, 8, 64, 8);
    };

    // The three gentler settings share a bucket because the weight is divided away, exactly as it is
    // in vanilla: only hard clears the next step.
    CHECK(spreadsUnder("peaceful") == 6);
    CHECK(spreadsUnder("easy") == 6);
    CHECK(spreadsUnder("normal") == 6);
    CHECK(spreadsUnder("hard") == 7);
}

TEST_CASE("what catches is read off all six sides of the empty space") {
    // One plank on each side of the same candidate in turn, so every lookup is exercised on its own.
    CHECK(firstCatch({9, 65, 8}, {9, 66, 8}) == 110);
    CHECK(firstCatch({9, 67, 8}, {9, 66, 8}) == 316);
    CHECK(firstCatch({9, 66, 7}, {9, 66, 8}) == 46);
    CHECK(firstCatch({9, 66, 9}, {9, 66, 8}) == 83);
    CHECK(firstCatch({8, 66, 8}, {9, 66, 8}) == 98);
    CHECK(firstCatch({10, 66, 8}, {9, 66, 8}) == 26);
}

TEST_CASE("the difficulty weight separates the settings the arithmetic can separate") {
    // Held at age nine so the divisor stays small enough for the weight to move the answer: at
    // fifteen it divides away, which is why the plank field only ever tells hard apart.
    const auto catchesUnder = [](std::int32_t age, const std::string& difficulty) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 63, 8, stateOf("netherrack")));
        REQUIRE(world.setBlockState(9, 65, 8, stateOf("oak_leaves")));
        rng::RandomSource random = rng::RandomSource::legacy(19);
        for (std::int32_t turn = 0; turn < 900; ++turn) {
            REQUIRE(world.setBlockState(8, 64, 8, fireAged(age)));
            static_cast<void>(fires::tick(world, kDry, difficulty, 8, 64, 8, random));
            const std::optional<std::int32_t> stateId = world.blockState(9, 66, 8);
            if (stateId.has_value() && fires::isFire(*stateId)) {
                return turn;
            }
        }
        return -1;
    };

    CHECK(catchesUnder(9, "peaceful") == 110);
    CHECK(catchesUnder(9, "easy") == 110);
    CHECK(catchesUnder(9, "normal") == 62);
    CHECK(catchesUnder(9, "hard") == 62);

    // A brand new fire divides by thirty instead, which is a different bucket again.
    CHECK(catchesUnder(0, "peaceful") == 62);
    CHECK(catchesUnder(0, "hard") == 69);
}

TEST_CASE("a plank beside the fire is either taken or left burning") {
    const auto burn = [](std::int64_t seed) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 63, 8, stateOf("netherrack")));
        REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
        REQUIRE(world.setBlockState(9, 64, 8, stateOf("oak_planks")));
        rng::RandomSource random = rng::RandomSource::legacy(seed);
        for (std::int32_t turn = 0; turn < 400; ++turn) {
            static_cast<void>(fires::tick(world, kDry, "hard", 8, 64, 8, random));
            if (world.blockState(9, 64, 8) != stateOf("oak_planks")) {
                return std::pair{turn, *world.blockState(9, 64, 8)};
            }
        }
        return std::pair{-1, 0};
    };

    const std::pair<std::int32_t, std::int32_t> taken = burn(2);
    CHECK(taken.first == 1);
    CHECK(taken.second == registry::blocks::air().stateId);

    // A young enough fire leaves one of its own behind instead, a step older than itself.
    const std::pair<std::int32_t, std::int32_t> left = burn(6);
    CHECK(left.first == 5);
    CHECK(fires::isFire(left.second));
    CHECK(propertyOf(left.second, "age") == "1");

    // Which of the two happens is its own roll, so forty seeds together pin how often a block is
    // left burning rather than taken, and how long the fire takes to reach it.
    std::int32_t leftBurning = 0;
    std::int32_t totalTurns = 0;
    std::int32_t totalAge = 0;
    for (std::int64_t seed = 0; seed < 40; ++seed) {
        const std::pair<std::int32_t, std::int32_t> outcome = burn(seed);
        if (fires::isFire(outcome.second)) {
            ++leftBurning;
            totalAge += std::stoi(propertyOf(outcome.second, "age"));
        }
        totalTurns += outcome.first;
    }
    CHECK(leftBurning == 10);
    CHECK(totalTurns == 557);
    // The age a left-behind fire starts at is the burning fire's own plus a fifth of a step, so the
    // ages together say more than any single one of them.
    CHECK(totalAge == 59);
}

TEST_CASE("the roof that counts is the one over the fire's own column") {
    // The roof sits over an odd column, so reading the heightmap one block off answers wrongly.
    WorldState world = emptyWorld();
    floor(world);
    REQUIRE(world.setBlockState(9, 64, 9, fireAged(0)));
    REQUIRE(world.setBlockState(9, 70, 9, stateOf("stone")));
    rng::RandomSource random = rng::RandomSource::legacy(11);

    std::int32_t turns = 0;
    while (world.blockState(9, 64, 9) != registry::blocks::air().stateId && turns < 100) {
        static_cast<void>(fires::tick(world, rain(), "normal", 9, 64, 9, random));
        ++turns;
    }
    CHECK(turns == 11);
}

TEST_CASE("the fire works on all six blocks touching it") {
    // A plank on one side at a time: the turn it goes pins both the offset and the odds that side
    // is rolled against, which is not the same up and down as it is sideways.
    const auto goes = [](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 63, 8, stateOf("netherrack")));
        REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
        REQUIRE(world.setBlockState(8 + dx, 64 + dy, 8 + dz, stateOf("oak_planks")));
        rng::RandomSource random = rng::RandomSource::legacy(19);
        for (std::int32_t turn = 0; turn < 400; ++turn) {
            static_cast<void>(fires::tick(world, kDry, "hard", 8, 64, 8, random));
            if (world.blockState(8 + dx, 64 + dy, 8 + dz) != stateOf("oak_planks")) {
                return turn;
            }
        }
        return -1;
    };

    CHECK(goes(1, 0, 0) == 20);
    CHECK(goes(-1, 0, 0) == 41);
    CHECK(goes(0, -1, 0) == 15);
    CHECK(goes(0, 1, 0) == 3);
    CHECK(goes(0, 0, -1) == 15);
    CHECK(goes(0, 0, 1) == 11);
}

TEST_CASE("an old fire over ground that will not burn goes out on a roll of four") {
    // The fuel beside it is put back every turn, so the only way out left is that roll.
    const auto lasts = [](const std::string& floorBlock) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        rng::RandomSource random = rng::RandomSource::legacy(19);
        for (std::int32_t turn = 0; turn < 200; ++turn) {
            REQUIRE(world.setBlockState(8, 63, 8, stateOf(floorBlock)));
            REQUIRE(world.setBlockState(8, 65, 8, stateOf("oak_planks")));
            REQUIRE(world.setBlockState(8, 64, 8, fireAged(15)));
            static_cast<void>(fires::tick(world, kDry, "normal", 8, 64, 8, random));
            if (world.blockState(8, 64, 8) == registry::blocks::air().stateId) {
                return turn;
            }
        }
        return -1;
    };

    CHECK(lasts("stone") == 4);
    // Ground that burns keeps it alive: the roll is only reached over something that will not.
    CHECK(lasts("oak_planks") == -1);
}

TEST_CASE("one flammable block on any side is enough to keep a fire going") {
    // Over stone the fire dies of old age unless something near it will burn, and the fuel is put
    // back every turn so only the question of whether it is seen is being asked.
    const auto lasts = [](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        WorldState world = emptyWorld();
        floor(world);
        REQUIRE(world.setBlockState(8, 64, 8, fireAged(15)));
        rng::RandomSource random = rng::RandomSource::legacy(19);
        for (std::int32_t turn = 0; turn < 400; ++turn) {
            REQUIRE(world.setBlockState(8 + dx, 64 + dy, 8 + dz, stateOf("oak_planks")));
            REQUIRE(world.setBlockState(8, 64, 8, fireAged(15)));
            static_cast<void>(fires::tick(world, kDry, "normal", 8, 64, 8, random));
            if (world.blockState(8, 64, 8) == registry::blocks::air().stateId) {
                return turn;
            }
        }
        return -1;
    };

    // Nothing to burn at all: the fire is gone the first turn it is older than three.
    CHECK(lasts(3, 0, 0) == 0);
    // Fuel underneath means the roll that kills an old fire is never even reached.
    CHECK(lasts(0, -1, 0) == -1);
    CHECK(lasts(0, 1, 0) == 4);
    CHECK(lasts(0, 0, -1) == 6);
    CHECK(lasts(0, 0, 1) == 4);
    CHECK(lasts(-1, 0, 0) == 6);
    CHECK(lasts(1, 0, 0) == 6);
}

TEST_CASE("the column the roof is read from is the one in the fire's own chunk") {
    WorldState world = emptyWorld();
    floor(world);
    world.getOrGenerateChunk(1, 0);
    for (std::int32_t x = 16; x < 32; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 63, z, stateOf("stone")));
        }
    }
    REQUIRE(world.setBlockState(20, 64, 9, fireAged(0)));
    REQUIRE(world.setBlockState(20, 70, 9, stateOf("stone")));
    rng::RandomSource random = rng::RandomSource::legacy(11);

    std::int32_t turns = 0;
    while (world.blockState(20, 64, 9) != registry::blocks::air().stateId && turns < 100) {
        static_cast<void>(fires::tick(world, rain(), "normal", 20, 64, 9, random));
        ++turns;
    }
    // Reading the heightmap from the chunk next door would find no roof and douse it in two.
    CHECK(turns == 11);
}

TEST_CASE("fire on magma and on bedrock burns forever too") {
    const auto survives = [](const std::string& floorBlock) {
        WorldState world = emptyWorld();
        floor(world, floorBlock);
        REQUIRE(world.setBlockState(8, 64, 8, fireAged(0)));
        rng::RandomSource random = rng::RandomSource::legacy(3);
        for (std::int32_t turn = 0; turn < 200; ++turn) {
            static_cast<void>(fires::tick(world, kDry, "normal", 8, 64, 8, random));
        }
        const std::optional<std::int32_t> stateId = world.blockState(8, 64, 8);
        return stateId.has_value() && fires::isFire(*stateId);
    };

    CHECK(survives("magma_block"));
    CHECK(survives("bedrock"));
    CHECK_FALSE(survives("stone"));
}
