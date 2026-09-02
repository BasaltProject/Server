#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/entity/EntityPhysics.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <doctest.h>

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

/// A dropped item, which is the smallest thing the physics is asked about.
[[nodiscard]] ServerEntity item(double x, double y, double z) {
    return ServerEntity{
        1, basalt::wire::common::Uuid::random(), basalt::wire::common::Identifier::vanilla("item"), 1, x, y, z};
}

constexpr double kItemGravity = 0.04;

} // namespace

TEST_CASE("nothing under it means it only falls faster") {
    Loaded loaded;
    ServerEntity dropped = item(8.5, 70.0, 8.5);

    entityPhysics::move(loaded.state, dropped, kItemGravity);

    // parity: gravity first, then the move, then the drag on what is left.
    CHECK(dropped.velocityY == doctest::Approx(-0.04 * 0.98));
    CHECK(dropped.y == doctest::Approx(70.0 - 0.04));
    CHECK_FALSE(dropped.onGround);
    CHECK(dropped.velocityX == doctest::Approx(0.0));
    // Only the axis that moved moved.
    CHECK(dropped.x == doctest::Approx(8.5));
    CHECK(dropped.z == doctest::Approx(8.5));
}

TEST_CASE("a fall that lands stops dead and says so") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 69, 8, stateOf("stone")));
    ServerEntity dropped = item(8.5, 70.01, 8.5);

    entityPhysics::move(loaded.state, dropped, kItemGravity);

    CHECK(dropped.onGround);
    CHECK(dropped.y == doctest::Approx(70.0));
    CHECK(dropped.velocityY == doctest::Approx(0.0));
}

TEST_CASE("a landing on slime throws it back instead of stopping it") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 69, 8, stateOf("slime_block")));
    ServerEntity dropped = item(8.5, 70.0, 8.5);
    dropped.velocityY = -0.5;

    entityPhysics::move(loaded.state, dropped, kItemGravity);

    // parity: four fifths of the block's own bounce, and what is not alive gets no more than that.
    CHECK(dropped.onGround);
    CHECK(dropped.velocityY > 0.0);
    CHECK(dropped.velocityY == doctest::Approx(0.54 * 0.8 * 0.98));
}

TEST_CASE("a landing on anything else keeps nothing of the fall") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 69, 8, stateOf("stone")));
    ServerEntity dropped = item(8.5, 70.0, 8.5);
    dropped.velocityY = -0.5;

    entityPhysics::move(loaded.state, dropped, kItemGravity);

    CHECK(dropped.onGround);
    CHECK(dropped.velocityY == doctest::Approx(0.0));
    CHECK(dropped.y == doctest::Approx(70.0));
}

TEST_CASE("the ground drag is only paid on the tick that lands") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 69, 8, stateOf("stone")));
    ServerEntity landed = item(8.5, 70.0, 8.5);
    landed.velocityX = 1.0;
    landed.velocityZ = 0.5;

    entityPhysics::move(loaded.state, landed, kItemGravity, 0.98, 0.7, -0.5);

    CHECK(landed.onGround);
    CHECK(landed.velocityX == doctest::Approx(1.0 * 0.98 * 0.7));
    CHECK(landed.velocityZ == doctest::Approx(0.5 * 0.98 * 0.7));

    ServerEntity falling = item(8.5, 75.0, 8.5);
    falling.velocityX = 1.0;
    entityPhysics::move(loaded.state, falling, kItemGravity, 0.98, 0.7, -0.5);
    CHECK_FALSE(falling.onGround);
    CHECK(falling.velocityX == doctest::Approx(1.0 * 0.98));
}

TEST_CASE("a move stopped sideways loses that axis and keeps the other") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(9, 70, 8, stateOf("stone")));
    ServerEntity dropped = item(8.5, 70.0, 8.5);
    dropped.velocityX = 1.0;
    dropped.velocityZ = 0.1;

    entityPhysics::move(loaded.state, dropped, 0.0, 1.0);

    CHECK(dropped.velocityX == doctest::Approx(0.0));
    CHECK(dropped.velocityZ == doctest::Approx(0.1));
    CHECK(dropped.x < 9.0);
    CHECK(dropped.z == doctest::Approx(8.6));
}

TEST_CASE("the block whose bounce is felt is the one half a block under the feet") {
    Loaded loaded;
    // Standing on the very top of a slime block, the probe still reaches it.
    REQUIRE(loaded.state.setBlockState(8, 69, 8, stateOf("slime_block")));
    ServerEntity onSlime = item(8.5, 70.0, 8.5);
    onSlime.velocityY = -0.5;
    entityPhysics::move(loaded.state, onSlime, kItemGravity);
    CHECK(onSlime.velocityY > 0.0);

    // A slime block one lower is out of the probe's reach, so the landing is a plain stop.
    Loaded other;
    REQUIRE(other.state.setBlockState(8, 68, 8, stateOf("slime_block")));
    REQUIRE(other.state.setBlockState(8, 69, 8, stateOf("stone")));
    ServerEntity onStone = item(8.5, 70.0, 8.5);
    onStone.velocityY = -0.5;
    entityPhysics::move(other.state, onStone, kItemGravity);
    CHECK(onStone.velocityY == doctest::Approx(0.0));
}
