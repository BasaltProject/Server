#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/FlyingPathFinder.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::entity;
using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

using Position = std::array<std::int32_t, 3>;

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() {
        state.getOrGenerateChunk(0, 0);
        state.getOrGenerateChunk(1, 0);
    }

    /// A floor of stone over the whole square, at y 63, so y 64 is where a mob walks.
    void floor(std::int32_t fromX, std::int32_t fromZ, std::int32_t toX, std::int32_t toZ) {
        for (std::int32_t x = fromX; x <= toX; ++x) {
            for (std::int32_t z = fromZ; z <= toZ; ++z) {
                REQUIRE(state.setBlockState(x, 63, z, stateOf("stone")));
            }
        }
    }

    void wall(std::int32_t x, std::int32_t z) {
        REQUIRE(state.setBlockState(x, 64, z, stateOf("stone")));
        REQUIRE(state.setBlockState(x, 65, z, stateOf("stone")));
    }
};

} // namespace

TEST_CASE("a mob needs something under it and two blocks of room to stand in") {
    Loaded loaded;
    loaded.floor(0, 0, 4, 4);

    CHECK(pathFinder::canStandAt(loaded.state, 2, 64, 2));
    // Nothing to stand on.
    CHECK_FALSE(pathFinder::canStandAt(loaded.state, 2, 70, 2));
    // Its feet are in a block.
    REQUIRE(loaded.state.setBlockState(2, 64, 2, stateOf("stone")));
    CHECK_FALSE(pathFinder::canStandAt(loaded.state, 2, 64, 2));
    // Its head is.
    REQUIRE(loaded.state.setBlockState(2, 64, 2, registry::blocks::air().stateId));
    REQUIRE(loaded.state.setBlockState(2, 65, 2, stateOf("stone")));
    CHECK_FALSE(pathFinder::canStandAt(loaded.state, 2, 64, 2));
    // And an unloaded chunk is nowhere to stand at all.
    CHECK_FALSE(pathFinder::canStandAt(loaded.state, 500, 64, 500));
}

TEST_CASE("a straight walk over open ground is the straight line") {
    Loaded loaded;
    loaded.floor(0, 0, 6, 6);

    const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 1, 64, 3, 4, 64, 3);

    REQUIRE(path.has_value());
    CHECK(*path == std::vector<Position>{{2, 64, 3}, {3, 64, 3}, {4, 64, 3}});
}

TEST_CASE("standing on the target already is a path of no steps") {
    Loaded loaded;
    loaded.floor(0, 0, 4, 4);

    const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 2, 64, 2, 2, 64, 2);

    REQUIRE(path.has_value());
    CHECK(path->empty());
}

TEST_CASE("a mob that is not standing anywhere has no path from there") {
    Loaded loaded;
    loaded.floor(0, 0, 4, 4);

    CHECK_FALSE(pathFinder::find(loaded.state, 2, 70, 2, 3, 64, 3).has_value());
}

TEST_CASE("a target nothing can reach answers with no path rather than the nearest thing") {
    Loaded loaded;
    loaded.floor(0, 0, 6, 6);
    // A wall right across the square, with no way round either end.
    for (std::int32_t z = 0; z <= 6; ++z) {
        loaded.wall(3, z);
    }

    CHECK_FALSE(pathFinder::find(loaded.state, 1, 64, 3, 5, 64, 3).has_value());
}

TEST_CASE("a wall with a gap is walked round through the gap") {
    Loaded loaded;
    loaded.floor(0, 0, 6, 6);
    for (std::int32_t z = 0; z <= 5; ++z) {
        loaded.wall(3, z);
    }

    const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 1, 64, 1, 5, 64, 1);

    REQUIRE(path.has_value());
    // Every step is somewhere it can stand, and none of them is inside the wall.
    for (const Position& at : *path) {
        CHECK(pathFinder::canStandAt(loaded.state, at[0], at[1], at[2]));
        const bool insideTheWall = at[0] == 3 && at[2] <= 5;
        CHECK_FALSE(insideTheWall);
    }
    CHECK(path->back() == Position{5, 64, 1});
}

TEST_CASE("a step of one block up is taken, and a wall of two is not") {
    Loaded loaded;
    loaded.floor(0, 0, 6, 6);
    // A ledge one block high across the middle, with room to stand on top of it.
    for (std::int32_t z = 0; z <= 6; ++z) {
        REQUIRE(loaded.state.setBlockState(3, 64, z, stateOf("stone")));
    }

    const std::optional<std::vector<Position>> up = pathFinder::find(loaded.state, 1, 64, 3, 5, 64, 3);
    REQUIRE(up.has_value());
    bool climbed = false;
    for (const Position& at : *up) {
        climbed = climbed || at[1] == 65;
    }
    CHECK(climbed);

    // One block higher and there is no room to stand on it, so the way is shut.
    for (std::int32_t z = 0; z <= 6; ++z) {
        REQUIRE(loaded.state.setBlockState(3, 65, z, stateOf("stone")));
    }
    CHECK_FALSE(pathFinder::find(loaded.state, 1, 64, 3, 5, 64, 3).has_value());
}

TEST_CASE("a drop of three is taken and a drop of four is not") {
    const auto reaches = [](std::int32_t depth) {
        Loaded loaded;
        loaded.floor(0, 0, 6, 6);
        // A trench one column wide, with its own floor `depth` blocks down.
        for (std::int32_t z = 0; z <= 6; ++z) {
            REQUIRE(loaded.state.setBlockState(3, 63, z, registry::blocks::air().stateId));
            REQUIRE(loaded.state.setBlockState(3, 63 - depth, z, stateOf("stone")));
        }
        return pathFinder::find(loaded.state, 1, 64, 3, 3, 64 - depth, 3).has_value();
    };

    // parity: three blocks is as far as a mob will step down.
    CHECK(reaches(1));
    CHECK(reaches(3));
    CHECK_FALSE(reaches(4));
}

TEST_CASE("a corner is only cut when both sides of it are open") {
    Loaded loaded;
    loaded.floor(0, 0, 4, 4);

    const std::optional<std::vector<Position>> open = pathFinder::find(loaded.state, 1, 64, 1, 2, 64, 2);
    REQUIRE(open.has_value());
    // Straight across the corner, in one step.
    CHECK(*open == std::vector<Position>{{2, 64, 2}});

    // With one side of the corner blocked it has to go the long way round.
    loaded.wall(2, 1);
    const std::optional<std::vector<Position>> blocked = pathFinder::find(loaded.state, 1, 64, 1, 2, 64, 2);
    REQUIRE(blocked.has_value());
    CHECK(blocked->size() == 2);
    CHECK(blocked->front() == Position{1, 64, 2});
}

TEST_CASE("a mob would rather walk round water than through it") {
    Loaded loaded;
    loaded.floor(0, 0, 8, 8);
    // A puddle right on the straight line between the two.
    for (std::int32_t x = 2; x <= 6; ++x) {
        REQUIRE(loaded.state.setBlockState(x, 64, 4, stateOf("water")));
    }

    const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 1, 64, 4, 7, 64, 4);

    REQUIRE(path.has_value());
    // parity: water costs eight, which is worth going four blocks out of the way to avoid.
    bool wetted = false;
    for (const Position& at : *path) {
        wetted = wetted || (at[2] == 4 && at[0] >= 2 && at[0] <= 6);
    }
    CHECK_FALSE(wetted);
    CHECK(path->back() == Position{7, 64, 4});
}

TEST_CASE("a mob would rather walk round fire than through it, and further round than water") {
    Loaded loaded;
    loaded.floor(0, 0, 8, 8);
    for (std::int32_t x = 2; x <= 6; ++x) {
        REQUIRE(loaded.state.setBlockState(x, 64, 4, stateOf("fire")));
    }
    // The only way round is through one square of water, which is the cheaper of the two.
    for (std::int32_t x = 0; x <= 8; ++x) {
        if (x != 4) {
            loaded.wall(x, 3);
            loaded.wall(x, 5);
        }
    }
    REQUIRE(loaded.state.setBlockState(4, 64, 3, stateOf("water")));

    const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 1, 64, 4, 7, 64, 4);

    REQUIRE(path.has_value());
    bool burnt = false;
    for (const Position& at : *path) {
        burnt = burnt || (at[2] == 4 && at[0] >= 2 && at[0] <= 6);
    }
    CHECK(burnt);
}

TEST_CASE("a search that runs out of nodes gives up rather than wandering forever") {
    Loaded loaded;
    // A ledge one block wide, so the only way along it is one step per node.
    for (std::int32_t chunkX = 0; chunkX < 20; ++chunkX) {
        loaded.state.getOrGenerateChunk(chunkX, 0);
    }
    for (std::int32_t x = 0; x <= 260; ++x) {
        REQUIRE(loaded.state.setBlockState(x, 63, 0, stateOf("stone")));
    }

    // parity: two hundred nodes and no more, so the far end of the ledge is out of reach.
    CHECK_FALSE(pathFinder::find(loaded.state, 0, 64, 0, 260, 64, 0).has_value());
    // The near end of the same ledge is found.
    const std::optional<std::vector<Position>> near = pathFinder::find(loaded.state, 0, 64, 0, 20, 64, 0);
    REQUIRE(near.has_value());
    CHECK(near->size() == 20);
}

TEST_CASE("every block that hurts is walked round, one at a time") {
    // parity: `getPathTypeStatic` names seven, of which only the ones with no shape of their own are
    // ever stood in: a mob walks over the top of a magma block or a campfire, where nothing is.
    for (const char* danger : {"lava", "fire", "soul_fire", "sweet_berry_bush"}) {
        REQUIRE(basalt::server::registry::blockShapes::isEmptyShape(stateOf(danger)));
        Loaded loaded;
        loaded.floor(0, 0, 8, 8);
        for (std::int32_t x = 2; x <= 6; ++x) {
            REQUIRE(loaded.state.setBlockState(x, 64, 4, stateOf(danger)));
        }

        const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 1, 64, 4, 7, 64, 4);

        REQUIRE(path.has_value());
        bool hurt = false;
        for (const Position& at : *path) {
            hurt = hurt || (at[1] == 64 && at[2] == 4 && at[0] >= 2 && at[0] <= 6);
        }
        CHECK_FALSE(hurt);
    }

    // And a block that costs nothing is walked straight over.
    Loaded plain;
    plain.floor(0, 0, 8, 8);
    for (std::int32_t x = 2; x <= 6; ++x) {
        REQUIRE(plain.state.setBlockState(x, 64, 4, stateOf("torch")));
    }
    const std::optional<std::vector<Position>> straight = pathFinder::find(plain.state, 1, 64, 4, 7, 64, 4);
    REQUIRE(straight.has_value());
    CHECK(straight->size() == 6);
}

TEST_CASE("a step up needs two blocks of room over the block it steps onto") {
    Loaded loaded;
    loaded.floor(0, 0, 6, 6);
    for (std::int32_t z = 0; z <= 6; ++z) {
        REQUIRE(loaded.state.setBlockState(3, 64, z, stateOf("stone")));
    }
    // A ceiling one block over the ledge: room for its feet and none for its head.
    for (std::int32_t z = 0; z <= 6; ++z) {
        REQUIRE(loaded.state.setBlockState(3, 66, z, stateOf("stone")));
    }

    CHECK_FALSE(pathFinder::find(loaded.state, 1, 64, 3, 5, 64, 3).has_value());

    // Lift the ceiling by one and the step is taken.
    for (std::int32_t z = 0; z <= 6; ++z) {
        REQUIRE(loaded.state.setBlockState(3, 66, z, registry::blocks::air().stateId));
    }
    CHECK(pathFinder::find(loaded.state, 1, 64, 3, 5, 64, 3).has_value());
}

TEST_CASE("a diagonal walk is one square at a time, not a leap") {
    Loaded loaded;
    loaded.floor(0, 0, 6, 6);

    const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 1, 64, 1, 3, 64, 3);

    REQUIRE(path.has_value());
    CHECK(*path == std::vector<Position>{{2, 64, 2}, {3, 64, 3}});
}

TEST_CASE("the node cap is two hundred, counted exactly") {
    Loaded loaded;
    for (std::int32_t chunkX = 0; chunkX < 20; ++chunkX) {
        loaded.state.getOrGenerateChunk(chunkX, 0);
    }
    for (std::int32_t x = 0; x <= 260; ++x) {
        REQUIRE(loaded.state.setBlockState(x, 63, 0, stateOf("stone")));
    }

    // One node per step along a ledge one block wide. The last node the search opens is the one it
    // answers with, so two hundred of them reach one short of that.
    std::int32_t furthest = 0;
    for (std::int32_t target = 1; target <= 260; ++target) {
        if (!pathFinder::find(loaded.state, 0, 64, 0, target, 64, 0).has_value()) {
            break;
        }
        furthest = target;
    }
    CHECK(furthest == 199);
}

TEST_CASE("the heuristic is what gets it across an open field inside its node budget") {
    Loaded loaded;
    for (std::int32_t chunkX = 0; chunkX <= 2; ++chunkX) {
        for (std::int32_t chunkZ = 0; chunkZ <= 2; ++chunkZ) {
            loaded.state.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    loaded.floor(0, 0, 40, 40);

    // parity: the weighted straight-line estimate is what keeps the search on the direct route. A
    // search that opened nodes by cost alone would fan out and run out long before thirty blocks.
    const std::optional<std::vector<Position>> path = pathFinder::find(loaded.state, 1, 64, 20, 31, 64, 20);

    REQUIRE(path.has_value());
    CHECK(path->size() == 30);
    CHECK(path->back() == Position{31, 64, 20});
}

TEST_CASE("a step up looks no higher than the two blocks it needs") {
    Loaded loaded;
    loaded.floor(0, 0, 6, 6);
    for (std::int32_t z = 0; z <= 6; ++z) {
        REQUIRE(loaded.state.setBlockState(3, 64, z, stateOf("stone")));
        // A ceiling three blocks over the ground, which is one more than the step needs.
        REQUIRE(loaded.state.setBlockState(3, 67, z, stateOf("stone")));
    }

    CHECK(pathFinder::find(loaded.state, 1, 64, 3, 5, 64, 3).has_value());
}

TEST_CASE("a flier's path goes over a wall it cannot pass and ends beside the target") {
    Loaded loaded;
    // A wall across the whole chunk, from y 64 up to 70, between x 3 and x 9.
    for (std::int32_t z = 0; z < 16; ++z) {
        for (std::int32_t y = 64; y <= 70; ++y) {
            REQUIRE(loaded.state.setBlockState(6, y, z, stateOf("stone")));
        }
    }
    const std::vector<flyingPathFinder::Node> path =
        flyingPathFinder::find(loaded.state, {.width = 1, .height = 4}, {3, 66, 8}, {9, 66, 8}, 640, 40.0);
    REQUIRE_FALSE(path.empty());
    const flyingPathFinder::Node& last = path.back();
    const std::int32_t off = std::abs(last[0] - 9) + std::abs(last[1] - 66) + std::abs(last[2] - 8);
    CHECK(off <= 1);
    // Four blocks tall, so it clears the top of the wall with its feet at 71.
    bool over = false;
    for (const flyingPathFinder::Node& node : path) {
        const bool clear = node[0] != 6 || node[1] >= 71;
        CHECK(clear);
        over = over || node[0] == 6;
    }
    CHECK(over);
}
