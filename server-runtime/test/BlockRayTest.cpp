#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>

#include <doctest.h>

#include <map>
#include <string>

using namespace basalt::server::runtime::entity;
using basalt::server::runtime::world::WorldState;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

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

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

} // namespace

TEST_CASE("a ray through empty air hits nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;

    CHECK_FALSE(blockRay::cast(world, 8.5, 64.5, 8.5, 12.5, 64.5, 8.5).has_value());
    // A ray that goes nowhere is not a ray.
    CHECK_FALSE(blockRay::cast(world, 8.5, 64.5, 8.5, 8.5, 64.5, 8.5).has_value());
}

TEST_CASE("a ray stops at the first block it meets, not the last") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(12, 64, 8, stateOf("stone")));

    const std::optional<BlockHit> hit = blockRay::cast(world, 8.5, 64.5, 8.5, 13.5, 64.5, 8.5);

    REQUIRE(hit.has_value());
    CHECK(hit->blockX == 10);
    CHECK(hit->blockY == 64);
    CHECK(hit->blockZ == 8);
    // It comes in through the west face, and lands exactly on it.
    CHECK(hit->face == registry::BlockFace::West);
    CHECK(hit->x == doctest::Approx(10.0));
    CHECK(hit->y == doctest::Approx(64.5));
}

TEST_CASE("the face a ray comes in through is the one it is coming at") {
    const auto faceFrom = [](double dx, double dy, double dz) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
        const std::optional<BlockHit> hit =
            blockRay::cast(world, 8.5 - (dx * 3), 64.5 - (dy * 3), 8.5 - (dz * 3), 8.5, 64.5, 8.5);
        REQUIRE(hit.has_value());
        return hit->face;
    };

    CHECK(faceFrom(1, 0, 0) == registry::BlockFace::West);
    CHECK(faceFrom(-1, 0, 0) == registry::BlockFace::East);
    CHECK(faceFrom(0, 1, 0) == registry::BlockFace::Down);
    CHECK(faceFrom(0, -1, 0) == registry::BlockFace::Up);
    CHECK(faceFrom(0, 0, 1) == registry::BlockFace::North);
    CHECK(faceFrom(0, 0, -1) == registry::BlockFace::South);
}

TEST_CASE("a ray goes straight through what has no collision at all") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("torch")));
    REQUIRE(world.setBlockState(11, 64, 8, stateOf("short_grass")));
    REQUIRE(world.setBlockState(12, 64, 8, stateOf("stone")));

    const std::optional<BlockHit> hit = blockRay::cast(world, 8.5, 64.5, 8.5, 13.5, 64.5, 8.5);

    REQUIRE(hit.has_value());
    CHECK(hit->blockX == 12);
}

TEST_CASE("a slab is met at its own top, not at the block boundary") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::optional<std::int32_t> slab =
        registry::blockStateDefinitions::stateIdOf(basalt::wire::common::Identifier::vanilla("oak_slab"),
                                                   {{"type", "bottom"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(slab.has_value());
    REQUIRE(world.setBlockState(8, 64, 8, *slab));

    // Straight down onto the slab, which is only half a block tall.
    const std::optional<BlockHit> hit = blockRay::cast(world, 8.5, 66.0, 8.5, 8.5, 64.0, 8.5);

    REQUIRE(hit.has_value());
    CHECK(hit->blockY == 64);
    CHECK(hit->y == doctest::Approx(64.5));
    CHECK(hit->face == registry::BlockFace::Up);
}

TEST_CASE("a fence post is met at the post, and missed beside it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("oak_fence")));

    // The post is the middle quarter of the block, so the ray reaches it late.
    const std::optional<BlockHit> onPost = blockRay::cast(world, 8.5, 64.5, 8.5, 12.5, 64.5, 8.5);
    REQUIRE(onPost.has_value());
    CHECK(onPost->blockX == 10);
    CHECK(onPost->x == doctest::Approx(10.375));
    CHECK(onPost->face == registry::BlockFace::West);

    // And a ray along the edge of the block passes the post by.
    CHECK_FALSE(blockRay::cast(world, 8.5, 64.5, 8.1, 12.5, 64.5, 8.1).has_value());
    // The post stops at a foot and a half, so anything above it goes over.
    CHECK_FALSE(blockRay::cast(world, 8.5, 65.8, 8.5, 12.5, 65.8, 8.5).has_value());
}

TEST_CASE("a shape of two boxes is met at whichever of them comes first") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::optional<std::int32_t> stairs = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("oak_stairs"),
        {{"facing", "north"}, {"half", "bottom"}, {"shape", "straight"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(stairs.has_value());
    REQUIRE(world.setBlockState(8, 64, 8, *stairs));

    // Over the low half the ray falls to the step, half a block up.
    const std::optional<BlockHit> onStep = blockRay::cast(world, 8.5, 66.0, 8.9, 8.5, 64.0, 8.9);
    REQUIRE(onStep.has_value());
    CHECK(onStep->y == doctest::Approx(64.5));

    // Over the tall half it stops on top of the block.
    const std::optional<BlockHit> onTop = blockRay::cast(world, 8.5, 66.0, 8.1, 8.5, 64.0, 8.1);
    REQUIRE(onTop.has_value());
    CHECK(onTop->y == doctest::Approx(65.0));
}

TEST_CASE("a ray that ends inside a block still hits it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));

    const std::optional<BlockHit> hit = blockRay::cast(world, 8.5, 64.5, 8.5, 9.5, 64.5, 8.5);

    REQUIRE(hit.has_value());
    CHECK(hit->blockX == 9);
    CHECK(hit->x == doctest::Approx(9.0));
}

TEST_CASE("a ray already inside a block hits it where it starts") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));

    const std::optional<BlockHit> hit = blockRay::cast(world, 8.5, 64.5, 8.5, 12.5, 64.5, 8.5);

    REQUIRE(hit.has_value());
    CHECK(hit->blockX == 8);
    CHECK(hit->x == doctest::Approx(8.5));
}

TEST_CASE("a ray out of the loaded world hits nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;

    CHECK_FALSE(blockRay::cast(world, 300.5, 64.5, 300.5, 305.5, 64.5, 300.5).has_value());
}

TEST_CASE("a diagonal ray meets the block on the side it reaches first") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(10, 64, 10, stateOf("stone")));

    const std::optional<BlockHit> hit = blockRay::cast(world, 8.5, 64.5, 8.1, 11.5, 64.5, 10.9);

    REQUIRE(hit.has_value());
    CHECK(hit->blockX == 10);
    CHECK(hit->blockZ == 10);
    CHECK(hit->face == registry::BlockFace::North);
}
