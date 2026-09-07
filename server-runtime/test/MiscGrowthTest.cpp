#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/MiscGrowth.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
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

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : std::string{block->path()};
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

} // namespace

TEST_CASE("nylium under something opaque goes back to netherrack") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("warped_nylium")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));
    std::vector<BlockChange> changes;

    miscGrowth::nylium(world, 8, 64, 8, changes);

    CHECK(changes.size() == 1);
    CHECK(world.blockState(8, 64, 8) == stateOf("netherrack"));
}

TEST_CASE("nylium under the open sky stays put") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("crimson_nylium")));
    std::vector<BlockChange> changes;

    miscGrowth::nylium(world, 8, 64, 8, changes);

    CHECK(changes.empty());
    CHECK(world.blockState(8, 64, 8) == stateOf("crimson_nylium"));
}

TEST_CASE("water over nylium is not a roof") {
    // The only dampenings vanilla's blocks carry are none, one and full, so one is the interesting
    // case between air and stone.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("warped_nylium")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("water", {{"level", "0"}})));
    std::vector<BlockChange> changes;

    miscGrowth::nylium(world, 8, 64, 8, changes);

    CHECK(changes.empty());
    CHECK(world.blockState(8, 64, 8) == stateOf("warped_nylium"));
}

TEST_CASE("a lit redstone ore goes dark, and a dark one is left alone") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t lit = stateOf("redstone_ore", {{"lit", "true"}});
    const std::int32_t dark = stateOf("redstone_ore", {{"lit", "false"}});
    REQUIRE(world.setBlockState(8, 64, 8, lit));
    std::vector<BlockChange> changes;

    miscGrowth::redstoneOre(world, 8, 64, 8, lit, changes);
    CHECK(world.blockState(8, 64, 8) == dark);
    CHECK(changes.size() == 1);

    miscGrowth::redstoneOre(world, 8, 64, 8, dark, changes);
    CHECK(changes.size() == 1);
}

TEST_CASE("a berry bush ripens one stage at a time under enough light") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sweet_berry_bush", {{"age", "0"}})));
    rng::RandomSource random = rng::RandomSource::legacy(4);
    std::vector<BlockChange> changes;

    std::int32_t turns = 0;
    while (world.blockState(8, 64, 8) != stateOf("sweet_berry_bush", {{"age", "3"}}) && turns < 200) {
        miscGrowth::sweetBerries(world, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
        ++turns;
    }
    // One roll in five means it takes a couple of dozen turns to walk the three stages.
    CHECK(turns == 21);
    CHECK(changes.size() == 3);

    // Ripe is as far as it goes.
    miscGrowth::sweetBerries(world, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
    CHECK(changes.size() == 3);
}

TEST_CASE("a berry bush in the dark does not ripen") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sweet_berry_bush", {{"age", "0"}})));
    rng::RandomSource random = rng::RandomSource::legacy(4);
    std::vector<BlockChange> changes;

    for (std::int32_t turn = 0; turn < 200; ++turn) {
        miscGrowth::sweetBerries(world, 8, 64, 8, *world.blockState(8, 64, 8), random, changes, false);
    }
    CHECK(changes.empty());
}

TEST_CASE("budding amethyst walks a face from empty air to a cluster") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("budding_amethyst")));
    rng::RandomSource random = rng::RandomSource::legacy(6);
    std::vector<BlockChange> changes;

    const std::vector<std::string> steps{"small_amethyst_bud", "medium_amethyst_bud", "large_amethyst_bud",
                                         "amethyst_cluster"};
    std::size_t reached = 0;
    for (std::int32_t turn = 0; turn < 4000 && reached < steps.size(); ++turn) {
        const std::size_t before = changes.size();
        miscGrowth::buddingAmethyst(world, 8, 64, 8, random, changes);
        for (std::size_t i = before; i < changes.size(); ++i) {
            if (changes[i].x == 8 && changes[i].y == 65 && changes[i].z == 8) {
                CHECK(pathOf(changes[i].stateId) == steps[reached]);
                CHECK(propertyOf(changes[i].stateId, "facing") == "up");
                ++reached;
            }
        }
    }
    CHECK(reached == steps.size());
    CHECK(world.blockState(8, 65, 8) == stateOf("amethyst_cluster", {{"facing", "up"}, {"waterlogged", "false"}}));
}

TEST_CASE("a cluster is the end of it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("budding_amethyst")));
    const std::int32_t cluster = stateOf("amethyst_cluster", {{"facing", "up"}, {"waterlogged", "false"}});
    REQUIRE(world.setBlockState(8, 65, 8, cluster));
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 9, stateOf("stone")));
    rng::RandomSource random = rng::RandomSource::legacy(6);
    std::vector<BlockChange> changes;

    for (std::int32_t turn = 0; turn < 400; ++turn) {
        miscGrowth::buddingAmethyst(world, 8, 64, 8, random, changes);
    }
    CHECK(changes.empty());
    CHECK(world.blockState(8, 65, 8) == cluster);
}

TEST_CASE("a turtle egg on sand cracks twice and then hatches away") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("sand")));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("turtle_egg", {{"eggs", "1"}, {"hatch", "0"}})));
    rng::RandomSource random = rng::RandomSource::legacy(8);
    std::vector<BlockChange> changes;

    std::int32_t turns = 0;
    while (world.blockState(8, 64, 8) != registry::blocks::air().stateId && turns < 20000) {
        miscGrowth::turtleEgg(world, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
        ++turns;
    }
    CHECK(changes.size() == 3);
    CHECK(propertyOf(changes[0].stateId, "hatch") == "1");
    CHECK(propertyOf(changes[1].stateId, "hatch") == "2");
    CHECK(changes[2].stateId == registry::blocks::air().stateId);
    // Two thousandths a turn, so it takes a good while.
    CHECK(turns == 1507);
}

TEST_CASE("a turtle egg off the sand never hatches") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("stone")));
    const std::int32_t egg = stateOf("turtle_egg", {{"eggs", "1"}, {"hatch", "0"}});
    REQUIRE(world.setBlockState(8, 64, 8, egg));
    rng::RandomSource random = rng::RandomSource::legacy(8);
    std::vector<BlockChange> changes;

    for (std::int32_t turn = 0; turn < 20000; ++turn) {
        miscGrowth::turtleEgg(world, 8, 64, 8, egg, random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("a dried ghast that is wet or drying asks to be looked at again") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t dry =
        stateOf("dried_ghast", {{"hydration", "0"}, {"waterlogged", "false"}, {"facing", "north"}});
    const std::int32_t wet = stateOf("dried_ghast", {{"hydration", "0"}, {"waterlogged", "true"}, {"facing", "north"}});
    const std::int32_t drying =
        stateOf("dried_ghast", {{"hydration", "2"}, {"waterlogged", "false"}, {"facing", "north"}});
    REQUIRE(world.setBlockState(8, 64, 8, dry));

    miscGrowth::driedGhast(world, 8, 64, 8, dry);
    CHECK_FALSE(world.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::driedGhastTickType()));

    miscGrowth::driedGhast(world, 8, 64, 8, wet);
    CHECK(world.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::driedGhastTickType()));

    miscGrowth::driedGhast(world, 9, 64, 8, drying);
    CHECK(world.hasScheduledBlockTick(9, 64, 8, scheduledBlocks::driedGhastTickType()));

    // One step of hydration left is still drying.
    const std::int32_t nearlyDry =
        stateOf("dried_ghast", {{"hydration", "1"}, {"waterlogged", "false"}, {"facing", "north"}});
    miscGrowth::driedGhast(world, 10, 64, 8, nearlyDry);
    CHECK(world.hasScheduledBlockTick(10, 64, 8, scheduledBlocks::driedGhastTickType()));
}

TEST_CASE("an eyeblossom follows the day timeline") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t open = stateOf("open_eyeblossom");
    const std::int32_t closed = stateOf("closed_eyeblossom");

    const auto turnAt = [&](std::int64_t totalTicks, std::int32_t stateId) {
        REQUIRE(world.setBlockState(8, 64, 8, stateId));
        std::vector<BlockChange> changes;
        miscGrowth::eyeblossom(world, WorldClock{totalTicks, 1.0F, false}, 8, 64, 8, stateId, changes);
        return world.blockState(8, 64, 8);
    };

    CHECK(turnAt(0, closed) == closed);
    CHECK(turnAt(0, open) == closed);
    CHECK(turnAt(12599, closed) == closed);
    CHECK(turnAt(12600, closed) == open);
    CHECK(turnAt(23400, closed) == open);
    CHECK(turnAt(23401, open) == closed);
    // A negative total tick is still somewhere in the day, and lands on the same keyframes.
    CHECK(turnAt(-11400, closed) == open);
    CHECK(turnAt(-11401, closed) == closed);
}

TEST_CASE("a potted eyeblossom turns in its pot") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t potted = stateOf("potted_closed_eyeblossom");
    REQUIRE(world.setBlockState(8, 64, 8, potted));
    std::vector<BlockChange> changes;

    miscGrowth::eyeblossom(world, WorldClock{13000, 1.0F, false}, 8, 64, 8, potted, changes);

    CHECK(world.blockState(8, 64, 8) == stateOf("potted_open_eyeblossom"));
    CHECK(changes.size() == 1);
}

TEST_CASE("a berry bush reads the light of the block over it, and wants nine") {
    // A torch five blocks away puts exactly nine on the space above the bush and eight on the bush
    // itself, so this pins both the threshold and which block is read.
    const auto ripensWith = [](std::int32_t torchY) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("sweet_berry_bush", {{"age", "0"}})));
        REQUIRE(world.setBlockState(13, torchY, 8, stateOf("torch")));
        rng::RandomSource random = rng::RandomSource::legacy(4);
        std::vector<BlockChange> changes;
        for (std::int32_t turn = 0; turn < 200; ++turn) {
            miscGrowth::sweetBerries(world, 8, 64, 8, *world.blockState(8, 64, 8), random, changes, false);
        }
        return changes.size();
    };

    CHECK(ripensWith(65) == 3);
    CHECK(ripensWith(64) == 0);
    CHECK(ripensWith(66) == 0);
}

TEST_CASE("budding amethyst buds on one roll in five") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("budding_amethyst")));
    rng::RandomSource random = rng::RandomSource::legacy(6);
    std::vector<BlockChange> changes;

    std::int32_t turns = 0;
    while (changes.empty() && turns < 200) {
        miscGrowth::buddingAmethyst(world, 8, 64, 8, random, changes);
        ++turns;
    }
    CHECK(turns == 9);
    CHECK(pathOf(changes[0].stateId) == "small_amethyst_bud");
}

TEST_CASE("a bud that grows in water comes out waterlogged") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("budding_amethyst")));
    for (const std::array<std::int32_t, 3>& side :
         {std::array<std::int32_t, 3>{8, 65, 8}, {8, 63, 8}, {7, 64, 8}, {9, 64, 8}, {8, 64, 7}, {8, 64, 9}}) {
        REQUIRE(world.setBlockState(side[0], side[1], side[2], stateOf("water", {{"level", "0"}})));
    }
    rng::RandomSource random = rng::RandomSource::legacy(6);
    std::vector<BlockChange> changes;

    while (changes.empty()) {
        miscGrowth::buddingAmethyst(world, 8, 64, 8, random, changes);
    }
    CHECK(pathOf(changes[0].stateId) == "small_amethyst_bud");
    CHECK(propertyOf(changes[0].stateId, "waterlogged") == "true");
}

TEST_CASE("a dried ghast asks to be looked at in five thousand ticks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t wet = stateOf("dried_ghast", {{"hydration", "0"}, {"waterlogged", "true"}, {"facing", "north"}});
    REQUIRE(world.setBlockState(8, 64, 8, wet));

    miscGrowth::driedGhast(world, 8, 64, 8, wet);
    for (std::int32_t turn = 0; turn < 4999; ++turn) {
        CHECK(world.dueBlockTicks().empty());
    }
    CHECK(world.dueBlockTicks().size() == 1);
}
