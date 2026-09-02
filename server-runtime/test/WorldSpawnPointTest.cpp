#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/WorldSpawnPoint.hpp>

#include <doctest.h>

#include <map>
#include <set>
#include <utility>

using basalt::server::world::Chunk;
namespace reg = basalt::server::registry;
namespace spawnPoint = basalt::server::runtime::world::worldSpawnPoint;

namespace {

/// A chunk with a floor of stone at `floorY` over the whole of it.
[[nodiscard]] Chunk floored(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t floorY) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; x++) {
        for (std::int32_t z = 0; z < 16; z++) {
            chunk.setBlockStateId(x, floorY, z, reg::blocks::stone().stateId);
        }
    }
    return chunk;
}

/// Chunks by position, generated flat and empty unless the test put one there.
struct World {
    std::map<std::pair<std::int32_t, std::int32_t>, Chunk> chunks;
    std::vector<std::pair<std::int32_t, std::int32_t>> asked;

    [[nodiscard]] spawnPoint::ChunkAt at() {
        return [this](std::int32_t chunkX, std::int32_t chunkZ) -> const Chunk& {
            asked.emplace_back(chunkX, chunkZ);
            const auto found = chunks.find({chunkX, chunkZ});
            if (found != chunks.end()) {
                return found->second;
            }
            return chunks.emplace(std::pair{chunkX, chunkZ}, Chunk{chunkX, chunkZ}).first->second;
        };
    }
};

} // namespace

TEST_CASE("the spawn is the block over the first solid column found") {
    World world;
    world.chunks.emplace(std::pair{0, 0}, floored(0, 0, 64));

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::find(0, 0, world.at());

    REQUIRE(found.has_value());
    // parity: `getSpawnPosInChunk` walks x first, so the first column of the chunk wins.
    CHECK((*found)[0] == 0);
    CHECK((*found)[1] == 65);
    CHECK((*found)[2] == 0);
}

TEST_CASE("a chunk's own coordinates are carried into the answer") {
    World world;
    world.chunks.emplace(std::pair{3, -2}, floored(3, -2, 70));

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::find(3, -2, world.at());

    REQUIRE(found.has_value());
    CHECK((*found)[0] == 48);
    CHECK((*found)[1] == 71);
    CHECK((*found)[2] == -32);
}

TEST_CASE("a column under water is refused, and a dry one beside it is taken") {
    Chunk chunk = floored(0, 0, 64);
    for (std::int32_t z = 0; z < 16; z++) {
        chunk.setBlockStateId(0, 65, z, reg::blocks::water().stateId);
    }
    World world;
    world.chunks.emplace(std::pair{0, 0}, std::move(chunk));

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::find(0, 0, world.at());

    REQUIRE(found.has_value());
    CHECK((*found)[0] == 1);
    CHECK((*found)[1] == 65);
}

TEST_CASE("an empty chunk holds nobody, and the spiral looks at its neighbours") {
    World world;
    world.chunks.emplace(std::pair{2, 0}, floored(2, 0, 64));

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::find(0, 0, world.at());

    REQUIRE(found.has_value());
    CHECK((*found)[0] == 32);
    CHECK(world.asked.size() > 1);
    CHECK(world.asked[0] == std::pair{0, 0});
}

TEST_CASE("the spiral gives up past five chunks out") {
    World world;
    world.chunks.emplace(std::pair{9, 0}, floored(9, 0, 64));

    CHECK_FALSE(spawnPoint::find(0, 0, world.at()).has_value());

    // Five out is still in reach.
    World closer;
    closer.chunks.emplace(std::pair{5, 0}, floored(5, 0, 64));
    CHECK(spawnPoint::find(0, 0, closer.at()).has_value());
    World tooFar;
    tooFar.chunks.emplace(std::pair{6, 0}, floored(6, 0, 64));
    CHECK_FALSE(spawnPoint::find(0, 0, tooFar.at()).has_value());
}

TEST_CASE("a world with nowhere to stand answers with nothing") {
    World world;

    CHECK_FALSE(spawnPoint::find(0, 0, world.at()).has_value());
    CHECK_FALSE(spawnPoint::find(0, 0, {}).has_value());
}

TEST_CASE("a chunk on its own is asked about directly") {
    const Chunk chunk = floored(0, 0, 100);

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::spawnInChunk(chunk);

    REQUIRE(found.has_value());
    CHECK((*found)[1] == 101);
    CHECK_FALSE(spawnPoint::spawnInChunk(Chunk{0, 0}).has_value());
}

TEST_CASE("the spiral walks every chunk within reach exactly once") {
    // parity: `setInitialSpawn`'s square spiral. Asking for each of the 121 chunks once and no
    // other is what says the turns are in the right places.
    World world;

    CHECK_FALSE(spawnPoint::find(0, 0, world.at()).has_value());

    REQUIRE(world.asked.size() == spawnPoint::kSpiralChunks * spawnPoint::kSpiralChunks);
    std::set<std::pair<std::int32_t, std::int32_t>> seen{world.asked.begin(), world.asked.end()};
    CHECK(seen.size() == world.asked.size());
    for (std::int32_t x = -spawnPoint::kReach; x <= spawnPoint::kReach; x++) {
        for (std::int32_t z = -spawnPoint::kReach; z <= spawnPoint::kReach; z++) {
            CHECK(seen.contains({x, z}));
        }
    }

    // It starts where it was told to and steps out from there.
    CHECK(world.asked[0] == std::pair{0, 0});
    CHECK(world.asked[1] == std::pair{1, 0});
}

TEST_CASE("the spiral is centred on the chunk it was given") {
    World world;

    CHECK_FALSE(spawnPoint::find(20, -30, world.at()).has_value());

    std::set<std::pair<std::int32_t, std::int32_t>> seen{world.asked.begin(), world.asked.end()};
    CHECK(seen.contains({20, -30}));
    CHECK(seen.contains({20 + spawnPoint::kReach, -30 + spawnPoint::kReach}));
    CHECK_FALSE(seen.contains({20 + spawnPoint::kReach + 1, -30}));
}

TEST_CASE("the last column of a chunk is searched like the first") {
    Chunk chunk{0, 0};
    chunk.setBlockStateId(15, 64, 15, reg::blocks::stone().stateId);

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::spawnInChunk(chunk);

    REQUIRE(found.has_value());
    CHECK((*found)[0] == 15);
    CHECK((*found)[1] == 65);
    CHECK((*found)[2] == 15);
}

TEST_CASE("a floor on the world's own floor still holds a player") {
    Chunk chunk{0, 0};
    const std::int32_t minY = chunk.verticalBounds().minY();
    chunk.setBlockStateId(0, minY, 0, reg::blocks::stone().stateId);

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::spawnInChunk(chunk);

    REQUIRE(found.has_value());
    CHECK((*found)[1] == minY + 1);
}

TEST_CASE("a column with water over its floor is refused even where the floor is fine") {
    Chunk chunk{0, 0};
    for (std::int32_t z = 0; z < 16; z++) {
        for (std::int32_t x = 0; x < 16; x++) {
            chunk.setBlockStateId(x, 64, z, reg::blocks::stone().stateId);
        }
    }
    // parity: `getLevelRespawnPos` walks down from the top and gives up the moment it meets fluid.
    chunk.setBlockStateId(0, 70, 0, reg::blocks::water().stateId);

    const std::optional<std::array<std::int32_t, 3>> found = spawnPoint::spawnInChunk(chunk);

    REQUIRE(found.has_value());
    CHECK((*found)[0] == 0);
    CHECK((*found)[2] == 1);
}
