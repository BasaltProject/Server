#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/VineGrowth.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t vineOn(const std::map<std::string, std::string>& faces) {
    std::map<std::string, std::string> all{
        {"up", "false"}, {"north", "false"}, {"south", "false"}, {"west", "false"}, {"east", "false"}};
    for (const auto& [face, on] : faces) {
        all[face] = on;
    }
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla("vine"), all, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] GameRuleValues spreading() { return GameRuleValues{}; }

} // namespace

TEST_CASE("a vine takes the face of the wall beside it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A wall to the north, and a vine hanging on nothing yet.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, vineOn({{"south", "true"}})));

    const GameRuleValues rules = spreading();
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000 && changes.empty(); ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    REQUIRE_FALSE(changes.empty());
    CHECK(propertyOf(*world.blockState(8, 64, 8), "north") == "true");
    // What it already had is still there.
    CHECK(propertyOf(*world.blockState(8, 64, 8), "south") == "true");
}

TEST_CASE("a vine climbs to the ceiling over it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, vineOn({{"north", "true"}})));

    const GameRuleValues rules = spreading();
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000 && changes.empty(); ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    REQUIRE_FALSE(changes.empty());
    CHECK(propertyOf(*world.blockState(8, 64, 8), "up") == "true");
}

TEST_CASE("how often a vine even tries") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, vineOn({{"south", "true"}})));

    const GameRuleValues rules = spreading();
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    std::int32_t calls = 0;
    while (changes.empty() && calls < 5000) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
        ++calls;
    }
    // parity: one roll in four before it even picks a direction, and then one direction in six.
    // Without the roll the same sequence would find its wall in a handful of ticks.
    REQUIRE_FALSE(changes.empty());
    CHECK(calls == 99);
}

TEST_CASE("a vine with nothing to hold on to stays where it is") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, vineOn({{"south", "true"}})));

    const GameRuleValues rules = spreading();
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    CHECK(changes.empty());

    // And a wall it cannot hold on to is no better: a torch is not a face.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("torch")));
    REQUIRE(world.setBlockState(8, 64, 9, stateOf("torch")));
    for (std::int32_t at = 0; at < 5000; ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("the face a vine needs is the one turned towards it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Stairs facing north have their full face on the north side, which is the side turned away
    // from a vine standing south of them: the vine needs their south face and there is none.
    const std::optional<std::int32_t> stairs = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("oak_stairs"),
        {{"facing", "north"}, {"half", "bottom"}, {"shape", "straight"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(stairs.has_value());
    REQUIRE(world.setBlockState(8, 64, 7, *stairs));
    // A top slab above: its underside is not a face either, whatever its top is.
    const std::optional<std::int32_t> slab = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("oak_slab"), {{"type", "top"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(slab.has_value());
    REQUIRE(world.setBlockState(8, 65, 8, *slab));
    REQUIRE(world.setBlockState(8, 64, 8, vineOn({{"south", "true"}})));

    const GameRuleValues rules = spreading();
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("the rule that switches vines off") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, vineOn({{"south", "true"}})));

    GameRuleValues rules;
    rules.set(gameRules::kSpreadVines, "false");
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    CHECK(changes.empty());
    // parity: it is on by default, which is why a jungle keeps growing over itself.
    CHECK(spreading().get(gameRules::kSpreadVines));
}

TEST_CASE("a crowd of vines stops spreading") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, vineOn({{"south", "true"}})));
    // parity: five of its own kind within four blocks and it stops. Four more plus itself is five.
    REQUIRE(world.setBlockState(6, 64, 8, vineOn({{"south", "true"}})));
    REQUIRE(world.setBlockState(10, 64, 8, vineOn({{"south", "true"}})));
    REQUIRE(world.setBlockState(8, 64, 10, vineOn({{"south", "true"}})));
    REQUIRE(world.setBlockState(8, 65, 8, vineOn({{"south", "true"}})));

    const GameRuleValues rules = spreading();
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    CHECK(changes.empty());

    // One fewer and it spreads again.
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::air().stateId));
    for (std::int32_t at = 0; at < 5000 && changes.empty(); ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    CHECK_FALSE(changes.empty());
}

TEST_CASE("a face a vine already has is not taken twice") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Walls on every side it could take, and a vine already holding all of them.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 9, stateOf("stone")));
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(
        8, 64, 8, vineOn({{"north", "true"}, {"south", "true"}, {"west", "true"}, {"east", "true"}, {"up", "true"}})));

    const GameRuleValues rules = spreading();
    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        vineGrowth::vine(world, rules, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    }
    CHECK(changes.empty());
}
