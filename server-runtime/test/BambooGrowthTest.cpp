#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BambooGrowth.hpp>

#include <doctest.h>

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

[[nodiscard]] std::int32_t stalkOf(const std::string& age, const std::string& leaves, const std::string& stage) {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("bamboo"), {{"age", age}, {"leaves", leaves}, {"stage", stage}}, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// Grows until something happens or the rolls run out, since the odds are one in three.
void growUntilChanged(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, rng::RandomSource& random,
                      std::vector<BlockChange>& changes) {
    for (std::int32_t at = 0; at < 500 && changes.empty(); ++at) {
        bambooGrowth::bamboo(world, x, y, z, *world.blockState(x, y, z), random, changes);
    }
}

} // namespace

TEST_CASE("a sapling puts a stalk over itself") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stateOf("bamboo_sapling")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 500 && changes.empty(); ++at) {
        bambooGrowth::bambooSapling(world, 4, 64, 4, random, changes);
    }
    REQUIRE(changes.size() == 1);
    CHECK(pathOf(*world.blockState(4, 65, 4)) == "bamboo");
    // The sapling itself is left where it is: the stalk grows out of it.
    CHECK(pathOf(*world.blockState(4, 64, 4)) == "bamboo_sapling");
}

TEST_CASE("a stalk climbs and carries its leaves with it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stalkOf("0", "none", "0")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    growUntilChanged(world, 4, 64, 4, random, changes);
    REQUIRE_FALSE(changes.empty());
    // The first stalk above a bare one gets small leaves.
    CHECK(propertyOf(*world.blockState(4, 65, 4), "leaves") == "small");

    changes.clear();
    growUntilChanged(world, 4, 65, 4, random, changes);
    REQUIRE_FALSE(changes.empty());
    // parity: the one below still has no leaves, so this one is small too rather than large.
    CHECK(propertyOf(*world.blockState(4, 66, 4), "leaves") == "small");

    changes.clear();
    growUntilChanged(world, 4, 66, 4, random, changes);
    REQUIRE_FALSE(changes.empty());
    // Now the one below has leaves, so the new tip takes the large ones and the two under it give
    // theirs up: the one right below drops to small and the one under that to none.
    CHECK(propertyOf(*world.blockState(4, 67, 4), "leaves") == "large");
    CHECK(propertyOf(*world.blockState(4, 66, 4), "leaves") == "small");
    CHECK(propertyOf(*world.blockState(4, 65, 4), "leaves") == "small");
    CHECK(propertyOf(*world.blockState(4, 64, 4), "leaves") == "none");

    changes.clear();
    growUntilChanged(world, 4, 67, 4, random, changes);
    REQUIRE_FALSE(changes.empty());
    // The two stripped are the two below whichever stalk grew, not the two below the new tip.
    CHECK(propertyOf(*world.blockState(4, 68, 4), "leaves") == "large");
    CHECK(propertyOf(*world.blockState(4, 67, 4), "leaves") == "large");
    CHECK(propertyOf(*world.blockState(4, 66, 4), "leaves") == "small");
    CHECK(propertyOf(*world.blockState(4, 65, 4), "leaves") == "none");

    changes.clear();
    growUntilChanged(world, 4, 68, 4, random, changes);
    REQUIRE_FALSE(changes.empty());
    // And now the crown really has moved: what was large is small and what was small is bare.
    CHECK(propertyOf(*world.blockState(4, 69, 4), "leaves") == "large");
    CHECK(propertyOf(*world.blockState(4, 67, 4), "leaves") == "small");
    CHECK(propertyOf(*world.blockState(4, 66, 4), "leaves") == "none");
}

TEST_CASE("a stalk that has finished never grows again") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 4, stalkOf("1", "none", "1")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 500; ++at) {
        bambooGrowth::bamboo(world, 4, 64, 4, *world.blockState(4, 64, 4), random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("bamboo stops at sixteen and under anything in the way") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Fifteen below plus this one is sixteen, which is as tall as it gets.
    for (std::int32_t at = 0; at < 15; ++at) {
        REQUIRE(world.setBlockState(4, 64 + at, 4, stalkOf("1", "none", "0")));
    }
    REQUIRE(world.setBlockState(4, 79, 4, stalkOf("1", "large", "0")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 500; ++at) {
        bambooGrowth::bamboo(world, 4, 79, 4, *world.blockState(4, 79, 4), random, changes);
    }
    CHECK(changes.empty());

    // And a stalk with something over it does not grow either, however much light gets through:
    // glass is bright and still in the way.
    REQUIRE(world.setBlockState(6, 64, 6, stalkOf("0", "none", "0")));
    REQUIRE(world.setBlockState(6, 65, 6, stateOf("glass")));
    for (std::int32_t at = 0; at < 500; ++at) {
        bambooGrowth::bamboo(world, 6, 64, 6, *world.blockState(6, 64, 6), random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("bamboo in the dark stays where it is") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // In the middle of the chunk, where a roof over it really is dark: the light reaches in from
    // the open sky at the borders, so a corner is still bright enough to grow in.
    REQUIRE(world.setBlockState(8, 64, 8, stalkOf("0", "none", "0")));
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("bamboo_sapling")));
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 70, z, stateOf("stone")));
        }
    }

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 500; ++at) {
        bambooGrowth::bamboo(world, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
        bambooGrowth::bambooSapling(world, 8, 66, 8, random, changes);
    }
    CHECK(changes.empty());
}
