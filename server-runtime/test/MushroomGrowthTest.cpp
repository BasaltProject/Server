#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/MushroomGrowth.hpp>

#include <doctest.h>

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

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A dark floor of mycelium with a roof over it, which is where mushrooms live.
void cave(WorldState& world) {
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 63, z, stateOf("mycelium")));
            REQUIRE(world.setBlockState(x, 70, z, stateOf("stone")));
        }
    }
}

[[nodiscard]] std::int32_t mushroomsAround(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                           std::int32_t stateId) {
    std::int32_t found = 0;
    for (std::int32_t dx = -5; dx <= 5; ++dx) {
        for (std::int32_t dy = -2; dy <= 2; ++dy) {
            for (std::int32_t dz = -5; dz <= 5; ++dz) {
                if (world.blockState(x + dx, y + dy, z + dz) == stateId) {
                    ++found;
                }
            }
        }
    }
    return found;
}

} // namespace

TEST_CASE("a mushroom spreads to a dark spot near it") {
    WorldState world = emptyWorld();
    cave(world);
    const std::int32_t brown = stateOf("brown_mushroom");
    REQUIRE(world.setBlockState(8, 64, 8, brown));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000 && changes.empty(); ++at) {
        mushroomGrowth::mushroom(world, 8, 64, 8, brown, random, changes);
    }
    REQUIRE_FALSE(changes.empty());
    CHECK(changes.front().stateId == brown);
    // It landed somewhere else, on the floor, within the reach of the walk.
    CHECK(std::abs(changes.front().x - 8) <= 4);
    CHECK(std::abs(changes.front().z - 8) <= 4);
    CHECK(changes.front().y == 64);
}

TEST_CASE("a mushroom tries about once in twenty-five ticks") {
    WorldState world = emptyWorld();
    cave(world);
    const std::int32_t brown = stateOf("brown_mushroom");
    REQUIRE(world.setBlockState(8, 64, 8, brown));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    std::int32_t calls = 0;
    while (changes.empty() && calls < 5000) {
        mushroomGrowth::mushroom(world, 8, 64, 8, brown, random, changes);
        ++calls;
    }
    REQUIRE_FALSE(changes.empty());
    // parity: one roll in twenty-five before it even looks, so a mushroom field fills a cave over
    // minutes and not over a second.
    CHECK(calls > 5);
}

TEST_CASE("the walk carries a mushroom further than one block") {
    WorldState world = emptyWorld();
    cave(world);
    const std::int32_t brown = stateOf("brown_mushroom");

    // parity: four hops, each starting from the last square that would hold it. One hop reaches two
    // blocks at most (the hop that advances, plus the one that lands), so three is the distance
    // that says the walk really is four long.
    rng::RandomSource random = rng::RandomSource::legacy(0);
    bool landedFar = false;
    for (std::int32_t attempt = 0; attempt < 60 && !landedFar; ++attempt) {
        WorldState field = emptyWorld();
        cave(field);
        REQUIRE(field.setBlockState(8, 64, 8, brown));
        std::vector<BlockChange> changes;
        for (std::int32_t at = 0; at < 2000 && changes.empty(); ++at) {
            mushroomGrowth::mushroom(field, 8, 64, 8, brown, random, changes);
        }
        if (!changes.empty()) {
            landedFar = std::abs(changes.front().x - 8) >= 3 || std::abs(changes.front().z - 8) >= 3;
        }
    }
    CHECK(landedFar);
}

TEST_CASE("the walk can step down as well as along") {
    const std::int32_t brown = stateOf("brown_mushroom");
    rng::RandomSource random = rng::RandomSource::legacy(0);
    bool landedLower = false;
    for (std::int32_t attempt = 0; attempt < 60 && !landedLower; ++attempt) {
        WorldState world = emptyWorld();
        cave(world);
        // A step down to the east: the floor there is one block lower, so only a walk that drifts
        // downwards ever lands on it.
        for (std::int32_t x = 10; x < 14; ++x) {
            for (std::int32_t z = 4; z < 13; ++z) {
                REQUIRE(world.setBlockState(x, 63, z, registry::blocks::air().stateId));
                REQUIRE(world.setBlockState(x, 62, z, stateOf("mycelium")));
            }
        }
        REQUIRE(world.setBlockState(8, 64, 8, brown));

        std::vector<BlockChange> changes;
        for (std::int32_t at = 0; at < 2000 && changes.empty(); ++at) {
            mushroomGrowth::mushroom(world, 8, 64, 8, brown, random, changes);
        }
        if (!changes.empty()) {
            landedLower = changes.front().y == 63;
        }
    }
    CHECK(landedLower);
}

TEST_CASE("a crowd of mushrooms stops spreading") {
    WorldState world = emptyWorld();
    cave(world);
    const std::int32_t brown = stateOf("brown_mushroom");
    // parity: five of its own kind in the nine by three by nine box and it stops.
    REQUIRE(world.setBlockState(8, 64, 8, brown));
    REQUIRE(world.setBlockState(6, 64, 8, brown));
    REQUIRE(world.setBlockState(10, 64, 8, brown));
    REQUIRE(world.setBlockState(8, 64, 6, brown));
    REQUIRE(world.setBlockState(8, 64, 10, brown));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        mushroomGrowth::mushroom(world, 8, 64, 8, brown, random, changes);
    }
    CHECK(changes.empty());
    CHECK(mushroomsAround(world, 8, 64, 8, brown) == 5);

    // One fewer and it spreads again.
    REQUIRE(world.setBlockState(8, 64, 10, registry::blocks::air().stateId));
    for (std::int32_t at = 0; at < 5000 && changes.empty(); ++at) {
        mushroomGrowth::mushroom(world, 8, 64, 8, brown, random, changes);
    }
    CHECK_FALSE(changes.empty());
}

TEST_CASE("mycelium holds a mushroom whatever the light") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `MushroomBlock.canSurvive` answers yes outright on the blocks mushrooms grow on, so
    // a mushroom field on mycelium spreads under an open sky just as it does in a cave.
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 63, z, stateOf("mycelium")));
        }
    }
    const std::int32_t brown = stateOf("brown_mushroom");
    REQUIRE(world.setBlockState(8, 64, 8, brown));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000 && changes.empty(); ++at) {
        mushroomGrowth::mushroom(world, 8, 64, 8, brown, random, changes);
    }
    CHECK_FALSE(changes.empty());
}

TEST_CASE("a mushroom does not grow into a block or onto nothing") {
    WorldState world = emptyWorld();
    cave(world);
    const std::int32_t red = stateOf("red_mushroom");
    REQUIRE(world.setBlockState(8, 64, 8, red));
    // Every square it could hop to is already taken.
    for (std::int32_t dx = -4; dx <= 4; ++dx) {
        for (std::int32_t dz = -4; dz <= 4; ++dz) {
            if (dx != 0 || dz != 0) {
                REQUIRE(world.setBlockState(8 + dx, 64, 8 + dz, stateOf("stone")));
            }
        }
    }

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        mushroomGrowth::mushroom(world, 8, 64, 8, red, random, changes);
    }
    CHECK(changes.empty());
}
