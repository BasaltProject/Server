#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::entity::ServerEntity;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
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

[[nodiscard]] std::function<std::int32_t()> ids() {
    return [next = std::make_shared<std::int32_t>(1)]() mutable { return (*next)++; };
}

} // namespace

TEST_CASE("which blocks fall and what gives way under them") {
    CHECK(fallingBlocks::falls(stateOf("sand")));
    CHECK(fallingBlocks::falls(stateOf("gravel")));
    CHECK(fallingBlocks::falls(stateOf("anvil")));
    CHECK_FALSE(fallingBlocks::falls(stateOf("stone")));
    CHECK_FALSE(fallingBlocks::falls(registry::blocks::air().stateId));

    // parity: `isFree` is air, anything replaceable and fire.
    CHECK(fallingBlocks::isFree(registry::blocks::air().stateId));
    CHECK(fallingBlocks::isFree(stateOf("water")));
    CHECK(fallingBlocks::isFree(stateOf("fire")));
    CHECK(fallingBlocks::isFree(stateOf("short_grass")));
    CHECK_FALSE(fallingBlocks::isFree(stateOf("stone")));
    CHECK_FALSE(fallingBlocks::isFree(stateOf("sand")));
}

TEST_CASE("a block that can fall books its own turn") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("sand")));
    REQUIRE(world.setBlockState(5, 70, 5, stateOf("stone")));

    fallingBlocks::scheduleIfFalling(world, 4, 70, 4);
    fallingBlocks::scheduleIfFalling(world, 5, 70, 5);
    CHECK(world.hasScheduledBlockTick(4, 70, 4, Identifier::vanilla("falling_block")));
    CHECK_FALSE(world.hasScheduledBlockTick(5, 70, 5, Identifier::vanilla("falling_block")));

    // parity: two ticks, so it comes due on the second and not the first.
    CHECK(world.dueBlockTicks().empty());
    CHECK(world.dueBlockTicks().size() == 1);
}

TEST_CASE("sand over a hole becomes an entity and lands as sand again") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 60, 4, stateOf("stone")));
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("sand")));

    ServerEntity* falling = fallingBlocks::beginFalling(world, 4, 70, 4, ids());
    REQUIRE(falling != nullptr);
    CHECK(falling->type() == Identifier::vanilla("falling_block"));
    // The block is gone from the world and its state is what the entity carries.
    CHECK(world.blockState(4, 70, 4) == registry::blocks::air().stateId);
    CHECK(falling->objectData == stateOf("sand"));
    // parity: it starts at the middle of the block it left.
    CHECK(falling->x == doctest::Approx(4.5));
    CHECK(falling->z == doctest::Approx(4.5));
    CHECK(falling->y == doctest::Approx(70.0));

    std::optional<BlockChange> landed;
    for (std::int32_t at = 0; at < 200 && !landed.has_value(); ++at) {
        landed = fallingBlocks::tick(world, *falling);
    }
    REQUIRE(landed.has_value());
    // It rests on the block that stopped it, not inside it.
    CHECK(landed->y == 61);
    CHECK(landed->stateId == stateOf("sand"));
    CHECK(world.blockState(4, 61, 4) == stateOf("sand"));
    CHECK(falling->removed);
}

TEST_CASE("it falls faster the longer it falls") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 10, 4, stateOf("stone")));
    REQUIRE(world.setBlockState(4, 200, 4, stateOf("gravel")));

    ServerEntity* falling = fallingBlocks::beginFalling(world, 4, 200, 4, ids());
    REQUIRE(falling != nullptr);

    // parity: `FallingBlockEntity.tick` takes gravity off and then scales by the air drag, so the
    // first tick moves it a little under a twenty-fifth of a block.
    CHECK_FALSE(fallingBlocks::tick(world, *falling).has_value());
    CHECK(falling->velocityY == doctest::Approx(-0.0392));
    CHECK(falling->y == doctest::Approx(199.9608));

    const double firstStep = 200.0 - falling->y;
    CHECK_FALSE(fallingBlocks::tick(world, *falling).has_value());
    const double secondStep = 199.9608 - falling->y;
    CHECK(secondStep > firstStep);
}

TEST_CASE("nothing falls when there is something under it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 69, 4, stateOf("stone")));
    REQUIRE(world.setBlockState(4, 70, 4, stateOf("sand")));

    CHECK(fallingBlocks::beginFalling(world, 4, 70, 4, ids()) == nullptr);
    // And it is still there.
    CHECK(world.blockState(4, 70, 4) == stateOf("sand"));

    // Nor does a block that never falls, nor an empty position.
    REQUIRE(world.setBlockState(6, 70, 6, stateOf("stone")));
    CHECK(fallingBlocks::beginFalling(world, 6, 70, 6, ids()) == nullptr);
    CHECK(fallingBlocks::beginFalling(world, 500, 70, 500, ids()) == nullptr);
}

TEST_CASE("a block that falls out of the world is gone") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, -60, 4, stateOf("sand")));

    ServerEntity* falling = fallingBlocks::beginFalling(world, 4, -60, 4, ids());
    REQUIRE(falling != nullptr);
    for (std::int32_t at = 0; at < 500 && !falling->removed; ++at) {
        CHECK_FALSE(fallingBlocks::tick(world, *falling).has_value());
    }
    CHECK(falling->removed);
    // Nothing was put back anywhere on the way down.
    CHECK(world.blockState(4, -64, 4) == registry::blocks::air().stateId);
}

TEST_CASE("a block resting on the floor of the world does not begin to fall") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, -64, 4, stateOf("sand")));
    CHECK(fallingBlocks::beginFalling(world, 4, -64, 4, ids()) == nullptr);
    CHECK(world.blockState(4, -64, 4) == stateOf("sand"));
}
