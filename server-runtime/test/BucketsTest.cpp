#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::protocol::play::Direction;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

struct Fixture {
    WorldState world = WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
    GameRuleValues gameRules;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    Fixture() {
        world.getOrGenerateChunk(0, 0);
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 0; z < 16; ++z) {
                REQUIRE(world.setBlockState(x, 63, z, stateOf("stone")));
            }
        }
    }

    [[nodiscard]] fluidFlow::Context context() {
        return fluidFlow::Context{.world = world, .gameRules = gameRules, .random = random, .ultrawarm = false};
    }
};

} // namespace

TEST_CASE("only the three buckets are buckets, and only two of them hold anything") {
    CHECK(buckets::isBucket(item("bucket")));
    CHECK(buckets::isBucket(item("water_bucket")));
    CHECK(buckets::isBucket(item("lava_bucket")));
    CHECK_FALSE(buckets::isBucket(item("milk_bucket")));
    CHECK_FALSE(buckets::isBucket(item("stone")));

    CHECK(buckets::contentOf(item("water_bucket")) == FluidKind::Water);
    CHECK(buckets::contentOf(item("lava_bucket")) == FluidKind::Lava);
    CHECK_FALSE(buckets::contentOf(item("bucket")).has_value());
    CHECK_FALSE(buckets::contentOf(item("milk_bucket")).has_value());
}

TEST_CASE("an empty bucket takes a source and comes back full") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));

    const std::optional<buckets::Result> result =
        buckets::useOn(fixture.context(), item("bucket"), 8, 64, 8, Direction::Up, false);

    REQUIRE(result.has_value());
    CHECK(result->replacement == item("water_bucket"));
    CHECK(fixture.world.blockState(8, 64, 8) == air());
    REQUIRE_FALSE(result->changes.empty());
    CHECK(result->changes[0].stateId == air());
    CHECK(result->changes[0].washedAway == stateOf("water"));
}

TEST_CASE("an empty bucket will not scoop a flow") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
    for (std::int32_t turn = 0; turn < 200; ++turn) {
        fixture.world.openTurn();
            for (const std::array<std::int32_t, 3>& position : fixture.world.dueFluidTicks()) {
            static_cast<void>(fluidFlow::tick(fixture.context(), position[0], position[1], position[2]));
        }
    }
    REQUIRE(fluidFlow::kindOf(*fixture.world.blockState(10, 64, 8)) == FluidKind::Water);

    CHECK_FALSE(buckets::useOn(fixture.context(), item("bucket"), 10, 64, 8, Direction::Up, false).has_value());
    CHECK(fluidFlow::kindOf(*fixture.world.blockState(10, 64, 8)) == FluidKind::Water);
}

TEST_CASE("a bucket emptied on a solid face pours onto the face, not into the block") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("stone")));

    const std::optional<buckets::Result> result =
        buckets::useOn(fixture.context(), item("water_bucket"), 8, 64, 8, Direction::Up, false);

    REQUIRE(result.has_value());
    CHECK(result->replacement == item("bucket"));
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("stone"));
    CHECK(fixture.world.blockState(8, 65, 8) == stateOf("water"));
}

TEST_CASE("a bucket emptied into something that gives way fills that block") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("short_grass")));

    const std::optional<buckets::Result> result =
        buckets::useOn(fixture.context(), item("water_bucket"), 8, 64, 8, Direction::Up, false);

    REQUIRE(result.has_value());
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("water"));
    CHECK(result->changes[0].washedAway == stateOf("short_grass"));
}

TEST_CASE("a bucket emptied onto something that is not solid takes its place") {
    Fixture fixture;
    // A torch is neither replaceable nor solid, which is the pair that decides this.
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("torch")));

    const std::optional<buckets::Result> result =
        buckets::useOn(fixture.context(), item("water_bucket"), 8, 64, 8, Direction::Up, false);

    REQUIRE(result.has_value());
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("water"));
    CHECK(result->changes[0].washedAway == stateOf("torch"));
}

TEST_CASE("a full bucket looks past what it cannot stand on") {
    Fixture fixture;
    // The torch is not part of the motion-blocking heightmap, so the trace goes straight through it
    // and lands on the stone floor under it.
    REQUIRE(fixture.world.setBlockState(8, 65, 8, stateOf("torch")));

    const std::optional<buckets::Result> result =
        buckets::use(fixture.context(), item("water_bucket"), 8.5, 67.5, 8.5, 0.0F, 90.0F, false);

    REQUIRE(result.has_value());
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("water"));
    CHECK(fixture.world.blockState(8, 65, 8) == stateOf("torch"));
}

TEST_CASE("in creative the bucket in the hand does not change") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));

    const std::optional<buckets::Result> filled =
        buckets::useOn(fixture.context(), item("bucket"), 8, 64, 8, Direction::Up, true);
    REQUIRE(filled.has_value());
    // Taking a source still tells the caller what was picked up: it is emptying that costs nothing.
    CHECK(filled->replacement == item("water_bucket"));

    REQUIRE(fixture.world.setBlockState(4, 64, 4, stateOf("stone")));
    const std::optional<buckets::Result> poured =
        buckets::useOn(fixture.context(), item("water_bucket"), 4, 64, 4, Direction::Up, true);
    REQUIRE(poured.has_value());
    CHECK_FALSE(poured->replacement.has_value());
}

TEST_CASE("an empty bucket stops at the first fluid the look vector meets") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));

    // Standing on the block, looking straight down at it.
    const std::optional<buckets::Result> result =
        buckets::use(fixture.context(), item("bucket"), 8.5, 66.5, 8.5, 0.0F, 90.0F, false);

    REQUIRE(result.has_value());
    CHECK(result->replacement == item("water_bucket"));
    CHECK(fixture.world.blockState(8, 64, 8) == air());
}

TEST_CASE("a full bucket traces straight through a fluid onto the floor under it") {
    Fixture fixture;
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));

    const std::optional<buckets::Result> result =
        buckets::use(fixture.context(), item("lava_bucket"), 8.5, 66.5, 8.5, 0.0F, 90.0F, false);

    REQUIRE(result.has_value());
    // The water is not a target, so the trace lands on the stone floor and the lava goes above it,
    // which is the block the water was in.
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("lava"));
}

TEST_CASE("nothing in reach means nothing happens") {
    Fixture fixture;

    // Looking up at an empty sky.
    CHECK_FALSE(buckets::use(fixture.context(), item("bucket"), 8.5, 70.5, 8.5, 0.0F, -90.0F, false).has_value());
    CHECK_FALSE(buckets::use(fixture.context(), item("water_bucket"), 8.5, 70.5, 8.5, 0.0F, -90.0F, false).has_value());
    CHECK(fixture.world.blockState(8, 71, 8) == air());
}

TEST_CASE("creative reaches half a block further") {
    const auto reaches = [](bool creative) {
        Fixture fixture;
        REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
        // Four and seven tenths up, which is past the hand and inside the creative reach.
        return buckets::use(fixture.context(), item("bucket"), 8.5, 69.7, 8.5, 0.0F, 90.0F, creative).has_value();
    };

    CHECK(reaches(true));
    CHECK_FALSE(reaches(false));
}

TEST_CASE("emptying a bucket wakes the fluid around what it filled") {
    Fixture fixture;

    const std::optional<buckets::Result> result =
        buckets::useOn(fixture.context(), item("water_bucket"), 8, 63, 8, Direction::Up, false);

    REQUIRE(result.has_value());
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("water"));
    // It spreads on its own from here, without anything else asking it to.
    for (std::int32_t turn = 0; turn < 200; ++turn) {
        fixture.world.openTurn();
            for (const std::array<std::int32_t, 3>& position : fixture.world.dueFluidTicks()) {
            static_cast<void>(fluidFlow::tick(fixture.context(), position[0], position[1], position[2]));
        }
    }
    CHECK(fluidFlow::kindOf(*fixture.world.blockState(11, 64, 8)) == FluidKind::Water);
}
