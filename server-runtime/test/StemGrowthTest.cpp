#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/StemGrowth.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;
using basalt::server::world::Chunk;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::int32_t agedStem(const std::string& path, std::int32_t age) {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), {{"age", std::to_string(age)}}, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

/// A fixed sequence, so a test that runs the growth often enough sees every roll it needs and sees
/// the same ones on every machine.
[[nodiscard]] rng::RandomSource fixed() { return rng::RandomSource::legacy(0); }

} // namespace

TEST_CASE("a stem ages one step at a time while it is young") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("farmland")));
    REQUIRE(world.setBlockState(8, 65, 8, agedStem("pumpkin_stem", 0)));

    rng::RandomSource random = fixed();
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000 && growthSupport::ageOf(*world.blockState(8, 65, 8)) < 7; ++at) {
        stemGrowth::stem(world, 8, 65, 8, *world.blockState(8, 65, 8), 1.0F, random, changes);
    }
    CHECK(growthSupport::ageOf(*world.blockState(8, 65, 8)) == 7);
    CHECK_FALSE(changes.empty());
}

TEST_CASE("a full grown stem puts a fruit beside it and turns to face it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Dirt all around, so wherever the roll points there is ground for a fruit.
    for (std::int32_t x = 6; x <= 10; ++x) {
        for (std::int32_t z = 6; z <= 10; ++z) {
            REQUIRE(world.setBlockState(x, 64, z, stateOf("dirt")));
        }
    }
    REQUIRE(world.setBlockState(8, 65, 8, agedStem("melon_stem", 7)));

    rng::RandomSource random = fixed();
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000 && changes.size() < 2; ++at) {
        stemGrowth::stem(world, 8, 65, 8, *world.blockState(8, 65, 8), 1.0F, random, changes);
    }
    REQUIRE(changes.size() >= 2);

    // The stem is now an attached one, and it faces the fruit.
    const std::int32_t stem = *world.blockState(8, 65, 8);
    const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stem);
    REQUIRE(block != nullptr);
    CHECK(block->path() == "attached_melon_stem");
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stem);
    const auto facing = properties.find("facing");
    REQUIRE(facing != properties.end());

    const std::int32_t fruitX = 8 + growthSupport::offsetX(facing->second);
    const std::int32_t fruitZ = 8 + growthSupport::offsetZ(facing->second);
    const std::optional<std::int32_t> fruit = world.blockState(fruitX, 65, fruitZ);
    REQUIRE(fruit.has_value());
    CHECK(*fruit == stateOf("melon"));
}

TEST_CASE("a fruit needs somewhere to sit") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Farmland only under the stem itself: stone all around is not ground a fruit grows on.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("farmland")));
    for (std::int32_t x = 6; x <= 10; ++x) {
        for (std::int32_t z = 6; z <= 10; ++z) {
            if (x != 8 || z != 8) {
                REQUIRE(world.setBlockState(x, 64, z, stateOf("stone")));
            }
        }
    }
    REQUIRE(world.setBlockState(8, 65, 8, agedStem("pumpkin_stem", 7)));

    rng::RandomSource random = fixed();
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        stemGrowth::stem(world, 8, 65, 8, *world.blockState(8, 65, 8), 1.0F, random, changes);
    }
    CHECK(changes.empty());
    // And the stem is still a stem.
    CHECK(registry::blockStateDefinitions::blockOf(*world.blockState(8, 65, 8))->path() == "pumpkin_stem");
}

TEST_CASE("a stem in the dark does not grow at all") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("farmland")));
    REQUIRE(world.setBlockState(8, 65, 8, agedStem("pumpkin_stem", 0)));
    // A roof over it: the light where it stands drops below the nine every crop needs.
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 70, z, stateOf("stone")));
        }
    }

    rng::RandomSource random = fixed();
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 50; ++at) {
        stemGrowth::stem(world, 8, 65, 8, *world.blockState(8, 65, 8), 1.0F, random, changes);
    }
    CHECK(changes.empty());
    CHECK(growthSupport::ageOf(*world.blockState(8, 65, 8)) == 0);
}

TEST_CASE("how often a stem grows is what the speed says") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("farmland")));
    REQUIRE(world.setBlockState(8, 65, 8, agedStem("pumpkin_stem", 0)));

    // parity: the roll is `nextInt(25 / speed + 1)`. At a speed of twenty-five that is one in two,
    // so a hundred ticks grow it about half the time: without the roll it would be every tick, and
    // without the plus one it would be one in one, which is also every tick.
    rng::RandomSource random = fixed();
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 100; ++at) {
        // Put it back to a young stem each time, so what is counted is the rolls and not the ages.
        REQUIRE(world.setBlockState(8, 65, 8, agedStem("pumpkin_stem", 0)));
        stemGrowth::stem(world, 8, 65, 8, *world.blockState(8, 65, 8), 25.0F, random, changes);
    }
    CHECK(changes.size() > 20);
    CHECK(changes.size() < 90);
}

TEST_CASE("a fruit does not push anything out of the way") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Ground all around, and something already standing on every side of the stem.
    for (std::int32_t x = 6; x <= 10; ++x) {
        for (std::int32_t z = 6; z <= 10; ++z) {
            REQUIRE(world.setBlockState(x, 64, z, stateOf("dirt")));
            if (x != 8 || z != 8) {
                REQUIRE(world.setBlockState(x, 65, z, stateOf("stone")));
            }
        }
    }
    REQUIRE(world.setBlockState(8, 65, 8, agedStem("pumpkin_stem", 7)));

    rng::RandomSource random = fixed();
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 5000; ++at) {
        stemGrowth::stem(world, 8, 65, 8, *world.blockState(8, 65, 8), 1.0F, random, changes);
    }
    CHECK(changes.empty());
    CHECK(*world.blockState(7, 65, 8) == stateOf("stone"));
    CHECK(*world.blockState(9, 65, 8) == stateOf("stone"));
}

TEST_CASE("cocoa grows through its three ages and stops") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::optional<std::int32_t> young = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("cocoa"), {{"age", "0"}}, std::nullopt);
    REQUIRE(young.has_value());
    REQUIRE(world.setBlockState(3, 64, 3, *young));

    rng::RandomSource random = fixed();
    std::vector<BlockChange> changes;
    std::int32_t calls = 0;
    while (changes.size() < 2 && calls < 5000) {
        cocoaGrowth::cocoa(world, 3, 64, 3, *world.blockState(3, 64, 3), random, changes);
        ++calls;
    }
    // parity: two is its last age, so it stops there rather than at whatever its states allow.
    CHECK(growthSupport::ageOf(*world.blockState(3, 64, 3)) == cocoaGrowth::kMaxAge);
    CHECK(changes.size() == 2);
    // One roll in five, so it took five ticks of this sequence rather than two: growing on every
    // tick is a cocoa pod that ripens in a tenth of a second.
    CHECK(calls == 5);

    for (std::int32_t at = 0; at < 100; ++at) {
        cocoaGrowth::cocoa(world, 3, 64, 3, *world.blockState(3, 64, 3), random, changes);
    }
    CHECK(changes.size() == 2);
}
