#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Composters.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::int32_t composterAt(std::int32_t level) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("composter"), {{"level", std::to_string(level)}}, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::int32_t levelOf(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> level = composters::levelAt(world, x, y, z);
    REQUIRE(level.has_value());
    return *level;
}

struct Loaded {
    WorldState state{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
    rng::RandomSource random = rng::RandomSource::legacy(7);

    Loaded() {
        state.getOrGenerateChunk(0, 0);
        REQUIRE(state.setBlockState(8, 64, 8, composterAt(0)));
    }
};

} // namespace

TEST_CASE("what a composter takes, and what it is worth") {
    // parity: the five `compostable/*` providers the items name. Spot checks at each of the five,
    // plus the shape of the table: everything else composts nothing.
    CHECK(composters::chanceOf(Identifier::vanilla("oak_leaves")) == 30);
    CHECK(composters::chanceOf(Identifier::vanilla("tall_grass")) == 50);
    CHECK(composters::chanceOf(Identifier::vanilla("apple")) == 65);
    CHECK(composters::chanceOf(Identifier::vanilla("hay_block")) == 85);
    CHECK(composters::chanceOf(Identifier::vanilla("cake")) == 100);
    CHECK_FALSE(composters::chanceOf(Identifier::vanilla("stone")).has_value());
    CHECK_FALSE(composters::chanceOf(Identifier::vanilla("bone_meal")).has_value());
}

TEST_CASE("the first layer always goes in and the rest are a roll") {
    Loaded loaded;
    // parity: `addItem` fills level zero for anything compostable without asking the random at all.
    const std::optional<BlockChange> first =
        composters::addItem(loaded.state, 8, 64, 8, Identifier::vanilla("oak_leaves"), loaded.random);
    REQUIRE(first.has_value());
    CHECK(levelOf(loaded.state, 8, 64, 8) == 1);

    // From there it is the item's own chance, so a run of leaves fills it in fits and starts. What
    // is pinned here is the ceiling: seven is as far as anything goes in.
    for (std::int32_t attempt = 0; attempt < 2000; ++attempt) {
        static_cast<void>(composters::addItem(loaded.state, 8, 64, 8, Identifier::vanilla("cake"), loaded.random));
    }
    CHECK(levelOf(loaded.state, 8, 64, 8) == composters::kFull);
    CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("composter")));

    // parity: `ComposterBlock.tick` is the seventh layer becoming the eighth, and nothing else is.
    const std::optional<BlockChange> ready = composters::settle(loaded.state, 8, 64, 8);
    REQUIRE(ready.has_value());
    CHECK(levelOf(loaded.state, 8, 64, 8) == composters::kReady);
    CHECK_FALSE(composters::settle(loaded.state, 8, 64, 8).has_value());

    // A composter that is ready takes nothing else in, and empties to zero.
    CHECK_FALSE(composters::addItem(loaded.state, 8, 64, 8, Identifier::vanilla("cake"), loaded.random).has_value());
    REQUIRE(composters::empty(loaded.state, 8, 64, 8).has_value());
    CHECK(levelOf(loaded.state, 8, 64, 8) == 0);
    CHECK_FALSE(composters::empty(loaded.state, 8, 64, 8).has_value());
}

TEST_CASE("a layer past the first is one draw of a hundred, and a sure one draws nothing") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 64, 8, composterAt(3)));
    loaded.random = rng::RandomSource::legacy(99);
    // Vector from the jar's own `compostable/low` provider at seed 99: 0 0 1 1 1 0, then this int.
    const bool expected[] = {false, false, true, true, true, false};
    for (const bool filled : expected) {
        CHECK(
            composters::addItem(loaded.state, 8, 64, 8, Identifier::vanilla("oak_leaves"), loaded.random).has_value() ==
            filled);
    }
    CHECK(levelOf(loaded.state, 8, 64, 8) == 6);
    // parity: `always_add_one` is a constant, so the cake takes nothing from the random.
    REQUIRE(composters::addItem(loaded.state, 8, 64, 8, Identifier::vanilla("cake"), loaded.random).has_value());
    CHECK(loaded.random.nextInt() == -571055948);
}

TEST_CASE("nothing that is not a composter answers any of this") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(9, 64, 8, registry::blocks::named("stone")->stateId));
    CHECK_FALSE(composters::levelAt(loaded.state, 9, 64, 8).has_value());
    CHECK_FALSE(composters::addItem(loaded.state, 9, 64, 8, Identifier::vanilla("cake"), loaded.random).has_value());
    CHECK_FALSE(composters::empty(loaded.state, 9, 64, 8).has_value());
    CHECK_FALSE(composters::settle(loaded.state, 9, 64, 8).has_value());
}
