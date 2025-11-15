#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/LavaGrowth.hpp>

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

/// How many of the blocks around a position caught fire after many ticks of lava.
[[nodiscard]] std::int32_t fires(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 std::int32_t reach) {
    std::int32_t found = 0;
    for (std::int32_t dx = -reach; dx <= reach; ++dx) {
        for (std::int32_t dy = -reach; dy <= reach; ++dy) {
            for (std::int32_t dz = -reach; dz <= reach; ++dz) {
                if (world.blockState(x + dx, y + dy, z + dz) == stateOf("fire")) {
                    ++found;
                }
            }
        }
    }
    return found;
}

} // namespace

TEST_CASE("lava lights the air over something that will burn") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("lava")));
    // Planks all around it at the same height: the air over them is what catches.
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            if (dx != 0 || dz != 0) {
                REQUIRE(world.setBlockState(8 + dx, 64, 8 + dz, stateOf("oak_planks")));
            }
        }
    }

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 200 && changes.empty(); ++at) {
        lavaGrowth::lava(world, 8, 64, 8, random, changes);
    }
    REQUIRE_FALSE(changes.empty());
    CHECK(changes.front().stateId == stateOf("fire"));
    CHECK(changes.front().y == 65);
    CHECK(fires(world, 8, 65, 8, 1) > 0);
}

TEST_CASE("fire needs the air over what burns, not the block itself") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("lava")));
    // Planks all around, and stone over every one of them: there is nowhere for fire to go.
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            if (dx != 0 || dz != 0) {
                REQUIRE(world.setBlockState(8 + dx, 64, 8 + dz, stateOf("oak_planks")));
            }
            REQUIRE(world.setBlockState(8 + dx, 65, 8 + dz, stateOf("stone")));
        }
    }

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 500; ++at) {
        lavaGrowth::lava(world, 8, 64, 8, random, changes);
    }
    CHECK(changes.empty());
    // And the stone is still stone: nothing was overwritten.
    CHECK(world.blockState(9, 65, 8) == stateOf("stone"));
}

TEST_CASE("one tick tries three places, not one") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("lava")));
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            if (dx != 0 || dz != 0) {
                REQUIRE(world.setBlockState(8 + dx, 64, 8 + dz, stateOf("oak_planks")));
            }
        }
    }

    // parity: the flat pass is three tries, so a single tick can light more than one fire. This
    // sequence lights two of them on its first tick that takes that path.
    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;
    bool litTwoAtOnce = false;
    for (std::int32_t at = 0; at < 200 && !litTwoAtOnce; ++at) {
        const std::size_t before = changes.size();
        lavaGrowth::lava(world, 8, 64, 8, random, changes);
        litTwoAtOnce = changes.size() - before >= 2;
        // Put the fires out again, so every tick has the same three places to choose from.
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                if (world.blockState(8 + dx, 65, 8 + dz) == stateOf("fire")) {
                    REQUIRE(world.setBlockState(8 + dx, 65, 8 + dz, registry::blocks::air().stateId));
                }
            }
        }
    }
    CHECK(litTwoAtOnce);
}

TEST_CASE("the climb wanders sideways as it goes up") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("lava")));
    // parity: each pass steps one block up and up to one block along each of x and z. The wool is
    // two along, so only a walk that drifts in x ever sees it.
    REQUIRE(world.setBlockState(10, 66, 8, stateOf("white_wool")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 4000 && changes.empty(); ++at) {
        lavaGrowth::lava(world, 8, 64, 8, random, changes);
    }
    REQUIRE_FALSE(changes.empty());
    CHECK(changes.front().x == 9);
    CHECK(changes.front().y == 66);
}

TEST_CASE("lava with nothing to burn lights nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("lava")));
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            if (dx != 0 || dz != 0) {
                REQUIRE(world.setBlockState(8 + dx, 64, 8 + dz, stateOf("stone")));
            }
        }
    }

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 500; ++at) {
        lavaGrowth::lava(world, 8, 64, 8, random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("lava climbs to what is above it, two blocks at most") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("lava")));
    // parity: the walk is `nextInt(3)` passes, so it climbs one or two and never three. Wool three
    // up is next to the air at two up, which is as far as the walk gets.
    REQUIRE(world.setBlockState(8, 67, 8, stateOf("white_wool")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 2000 && changes.empty(); ++at) {
        lavaGrowth::lava(world, 8, 64, 8, random, changes);
    }
    REQUIRE_FALSE(changes.empty());
    CHECK(changes.front().y > 64);
    CHECK(changes.front().y <= 66);

    // One block higher and the walk cannot reach it at all.
    WorldState far = emptyWorld();
    far.getOrGenerateChunk(0, 0);
    REQUIRE(far.setBlockState(8, 64, 8, stateOf("lava")));
    REQUIRE(far.setBlockState(8, 68, 8, stateOf("white_wool")));
    std::vector<BlockChange> none;
    for (std::int32_t at = 0; at < 2000; ++at) {
        lavaGrowth::lava(far, 8, 64, 8, random, none);
    }
    CHECK(none.empty());
}

TEST_CASE("a roof stops the walk upwards") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("lava")));
    // Wool up high, and a stone lid right over the lava: the walk stops at the lid.
    REQUIRE(world.setBlockState(8, 68, 8, stateOf("white_wool")));
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            REQUIRE(world.setBlockState(8 + dx, 65, 8 + dz, stateOf("stone")));
        }
    }

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 2000; ++at) {
        lavaGrowth::lava(world, 8, 64, 8, random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("lava at the edge of the world lights nothing it cannot see") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // The lava itself is loaded and everything around it is not.
    REQUIRE(world.setBlockState(0, 64, 0, stateOf("lava")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 500; ++at) {
        lavaGrowth::lava(world, 0, 64, 0, random, changes);
    }
    CHECK(changes.empty());
}
