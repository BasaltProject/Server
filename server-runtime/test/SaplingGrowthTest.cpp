#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/SaplingGrowth.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] std::int32_t saplingOf(const std::string& path, const std::string& stage) {
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), {{"stage", stage}}, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::int32_t countOf(const std::vector<BlockChange>& changes, const std::string& path) {
    std::int32_t total = 0;
    for (const BlockChange& change : changes) {
        if (pathOf(change.stateId) == path) {
            ++total;
        }
    }
    return total;
}

/// Which columns hold a log, which is what tells a branching tree from a straight one.
[[nodiscard]] std::set<std::pair<std::int32_t, std::int32_t>> logColumns(const std::vector<BlockChange>& changes,
                                                                         const std::string& path) {
    std::set<std::pair<std::int32_t, std::int32_t>> columns;
    for (const BlockChange& change : changes) {
        if (pathOf(change.stateId) == path) {
            columns.insert({change.x, change.z});
        }
    }
    return columns;
}

[[nodiscard]] std::int32_t lowestX(const std::vector<BlockChange>& changes) {
    std::int32_t lowest = std::numeric_limits<std::int32_t>::max();
    for (const BlockChange& change : changes) {
        lowest = std::min(lowest, change.x);
    }
    return lowest;
}

[[nodiscard]] std::int32_t highestX(const std::vector<BlockChange>& changes) {
    std::int32_t highest = std::numeric_limits<std::int32_t>::min();
    for (const BlockChange& change : changes) {
        highest = std::max(highest, change.x);
    }
    return highest;
}

/// Plants a sapling and takes its turn, which is what a random tick that passed its rolls does.
[[nodiscard]] std::vector<BlockChange> growAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              const std::string& path, std::int64_t seed) {
    const std::int32_t sapling = saplingOf(path, "1");
    REQUIRE(world.setBlockState(x, y, z, sapling));
    rng::RandomSource random = rng::RandomSource::legacy(seed);
    return saplingGrowth::advance(world, x, y, z, sapling, random);
}

} // namespace

TEST_CASE("every vanilla sapling is one and nothing else is") {
    for (const std::string& path : {"oak_sapling", "birch_sapling", "spruce_sapling", "jungle_sapling",
                                    "acacia_sapling", "dark_oak_sapling", "pale_oak_sapling", "cherry_sapling"}) {
        CHECK(saplingGrowth::isSapling(saplingOf(path, "0")));
        CHECK(saplingGrowth::isSapling(saplingOf(path, "1")));
    }
    CHECK(saplingGrowth::isSapling(stateOf("mangrove_propagule")));

    CHECK_FALSE(saplingGrowth::isSapling(stateOf("air")));
    CHECK_FALSE(saplingGrowth::isSapling(stateOf("dirt")));
    CHECK_FALSE(saplingGrowth::isSapling(stateOf("oak_log")));
    CHECK_FALSE(saplingGrowth::isSapling(stateOf("bamboo_sapling")));
    CHECK_FALSE(saplingGrowth::isSapling(-1));
}

TEST_CASE("a sapling at stage zero only advances its stage") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t young = saplingOf("oak_sapling", "0");
    const std::int32_t ready = saplingOf("oak_sapling", "1");
    REQUIRE(world.setBlockState(8, 64, 8, young));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    const std::vector<BlockChange> changes = saplingGrowth::advance(world, 8, 64, 8, young, random);

    REQUIRE(changes.size() == 1);
    CHECK(changes[0] == BlockChange{.x = 8, .y = 64, .z = 8, .stateId = ready});
    CHECK(world.blockState(8, 64, 8) == ready);
    // parity: the stage costs no roll, so the tree the next tick grows is the one it would have.
    CHECK(random.nextLong() == rng::RandomSource::legacy(0).nextLong());
}

TEST_CASE("a grown oak sapling becomes an oak") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> changes = growAt(world, 8, 64, 8, "oak_sapling", 0);

    CHECK(changes.size() == 63);
    CHECK(countOf(changes, "oak_log") == 5);
    CHECK(countOf(changes, "oak_leaves") == 57);
    // parity: `TreeFeature` puts dirt under the trunk, which is the one block below the sapling the
    // copied-back window has to reach.
    CHECK(countOf(changes, "dirt") == 1);
    CHECK(world.blockState(8, 63, 8) == stateOf("dirt"));
    CHECK(logColumns(changes, "oak_log") == std::set<std::pair<std::int32_t, std::int32_t>>{{8, 8}});
    CHECK(world.blockState(8, 64, 8) == stateOf("oak_log"));
    CHECK(world.blockState(8, 68, 8) == stateOf("oak_log"));
    CHECK(pathOf(*world.blockState(8, 69, 8)) == "oak_leaves");
}

TEST_CASE("an oak draws one in ten for the fancy variant") {
    // parity: 26.3 `TreeGrower.OAK`'s weights, nine to one: seed 9 draws the tenth and seed 0 does not.
    CHECK(rng::RandomSource::legacy(9).nextInt(10) == 9);
    CHECK(rng::RandomSource::legacy(0).nextInt(10) != 9);
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> fancy = growAt(world, 8, 64, 8, "oak_sapling", 9);

    CHECK(fancy.size() == 204);
    CHECK(countOf(fancy, "oak_log") == 13);
    CHECK(countOf(fancy, "oak_leaves") == 190);
    // The plain oak of seed 0 is one straight column; only the fancy one branches off the trunk.
    CHECK(logColumns(fancy, "oak_log").size() > 1);
}

TEST_CASE("a birch draws from its one-entry list like any other") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> changes = growAt(world, 8, 64, 8, "birch_sapling", 0);

    CHECK(changes.size() == 64);
    CHECK(countOf(changes, "birch_log") == 6);
    CHECK(countOf(changes, "birch_leaves") == 57);
    CHECK(countOf(changes, "dirt") == 1);
}

TEST_CASE("a tree grows where the sapling stood, not at the middle of the chunk") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> changes = growAt(world, 3, 64, 12, "spruce_sapling", 0);

    CHECK(logColumns(changes, "spruce_log") == std::set<std::pair<std::int32_t, std::int32_t>>{{3, 12}});
    CHECK(world.blockState(3, 63, 12) == stateOf("dirt"));
    CHECK(world.blockState(3, 64, 12) == stateOf("spruce_log"));
}

TEST_CASE("a canopy reaching the far column of the scratch chunk is still copied back") {
    // Seed 67's fancy oak spans seven blocks each way, the widest the window holds.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> changes = growAt(world, 8, 64, 8, "oak_sapling", 67);

    CHECK(changes.size() == 432);
    CHECK(lowestX(changes) == 1);
    CHECK(highestX(changes) == 15);
}

TEST_CASE("a wall in the far column keeps the canopy out of it") {
    // The same tree with the last column of the window walled off, which the copied-in blocks are
    // what tells the feature about: it grows around the wall instead of through it.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t y = 63; y <= 80; ++y) {
        for (std::int32_t z = 0; z <= 15; ++z) {
            REQUIRE(world.setBlockState(15, y, z, stateOf("stone")));
        }
    }
    const std::vector<BlockChange> changes = growAt(world, 8, 64, 8, "oak_sapling", 67);

    CHECK(changes.size() == 423);
    CHECK(highestX(changes) == 14);
}

TEST_CASE("what lies outside the loaded world reads as air") {
    // The same tree with its window half in the chunk to the west, which nobody has loaded: those
    // blocks read as air, so the canopy grows over them, and only the loaded half is written.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> changes = growAt(world, 2, 64, 8, "oak_sapling", 67);

    CHECK(changes.size() == 351);
    CHECK(lowestX(changes) == 0);
    CHECK(highestX(changes) == 9);
}

TEST_CASE("a sapling with no room above stays a sapling") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> changes = growAt(world, 8, 319, 8, "oak_sapling", 0);

    CHECK(changes.empty());
    CHECK(world.blockState(8, 319, 8) == saplingOf("oak_sapling", "1"));
}

TEST_CASE("a sapling on the floor of the world stays a sapling") {
    // The window would start below the world, and is clamped to the lowest block instead.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::vector<BlockChange> changes = growAt(world, 8, -64, 8, "oak_sapling", 0);

    CHECK(changes.empty());
    CHECK(world.blockState(8, -64, 8) == saplingOf("oak_sapling", "1"));
}

TEST_CASE("a sapling in a chunk that is not loaded grows nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t sapling = saplingOf("oak_sapling", "1");
    rng::RandomSource random = rng::RandomSource::legacy(0);

    CHECK(saplingGrowth::advance(world, 16, 64, 8, sapling, random).empty());
    CHECK(saplingGrowth::advance(world, 8, 64, 16, sapling, random).empty());
    CHECK_FALSE(world.isLoaded(1, 0));
    CHECK_FALSE(world.isLoaded(0, 1));
}

TEST_CASE("a block that is no sapling grows nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    rng::RandomSource random = rng::RandomSource::legacy(0);

    CHECK(saplingGrowth::advance(world, 8, 64, 8, stateOf("dirt"), random).empty());
    CHECK(saplingGrowth::advance(world, 8, 64, 8, stateOf("bamboo_sapling"), random).empty());
    CHECK(saplingGrowth::advance(world, 8, 64, 8, -1, random).empty());
    CHECK(world.blockState(8, 64, 8) == stateOf("air"));
}

TEST_CASE("a dark oak only grows from a square of four saplings") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    CHECK(growAt(world, 8, 64, 8, "dark_oak_sapling", 0).empty());

    const std::int32_t sapling = saplingOf("dark_oak_sapling", "1");
    REQUIRE(world.setBlockState(9, 64, 8, sapling));
    REQUIRE(world.setBlockState(8, 64, 9, sapling));
    REQUIRE(world.setBlockState(9, 64, 9, sapling));
    const std::vector<BlockChange> grown = growAt(world, 8, 64, 8, "dark_oak_sapling", 0);
    CHECK(countOf(grown, "dark_oak_log") > 4);
    CHECK(pathOf(*world.blockState(9, 64, 9)) == "dark_oak_log");
}

TEST_CASE("a poplar sapling grows one of the three poplars, drawn one in three") {
    // parity: 26.3 `TreeGrower.POPLAR`, red, orange and yellow at one weight each.
    const std::array<std::string, 3> leaves{"red_poplar_leaves", "orange_poplar_leaves", "yellow_poplar_leaves"};
    for (std::int64_t seed = 0; seed < 3; ++seed) {
        CAPTURE(seed);
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        const std::vector<BlockChange> changes = growAt(world, 8, 64, 8, "poplar_sapling", seed);
        const std::string& drawn = leaves[static_cast<std::size_t>(rng::RandomSource::legacy(seed).nextInt(3))];
        CHECK(countOf(changes, drawn) > 0);
        for (const std::string& other : leaves) {
            if (other != drawn) {
                CHECK(countOf(changes, other) == 0);
            }
        }
        CHECK(world.blockState(8, 64, 8) == stateOf("poplar_log"));
        // The side branches below the crown leave the trunk's own column.
        CHECK(logColumns(changes, "poplar_log").size() > 1);
    }
}
