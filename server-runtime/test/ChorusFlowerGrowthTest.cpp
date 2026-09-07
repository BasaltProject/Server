#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ChorusFlowerGrowth.hpp>

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

[[nodiscard]] std::int32_t flowerOf(std::int32_t age) {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("chorus_flower"), {{"age", std::to_string(age)}}, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string pathAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string ageAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("age");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// True when `used` is still where a source of the same seed starts, so no roll was spent.
[[nodiscard]] bool unrolled(rng::RandomSource& used, std::int64_t seed) {
    rng::RandomSource fresh = rng::RandomSource::legacy(seed);
    return used.nextInt() == fresh.nextInt();
}

void tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, rng::RandomSource& random,
          std::vector<BlockChange>& changes) {
    chorusFlowerGrowth::chorusFlower(world, x, y, z, *world.blockState(x, y, z), random, changes);
}

} // namespace

TEST_CASE("a flower on end stone climbs and leaves a stem behind") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(2)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);

    REQUIRE(changes.size() == 2);
    CHECK(pathAt(world, 8, 64, 8) == "chorus_plant");
    CHECK(pathAt(world, 8, 65, 8) == "chorus_flower");
    // parity: the flower carries its own age up with it, it does not age by climbing.
    CHECK(ageAt(world, 8, 65, 8) == "2");
    // Ground under the flower answers on its own, so nothing was rolled for.
    CHECK(unrolled(random, 0));
}

TEST_CASE("a flower under a block or already dead does nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(0)));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));

    // Right under the ceiling of the world nothing is air either, and the same check catches it.
    REQUIRE(world.setBlockState(4, 319, 4, flowerOf(0)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);
    tick(world, 4, 319, 4, random, changes);
    CHECK(changes.empty());

    // And an age five flower is finished wherever it stands.
    REQUIRE(world.setBlockState(2, 63, 2, stateOf("end_stone")));
    REQUIRE(world.setBlockState(2, 64, 2, flowerOf(chorusFlowerGrowth::kDeadAge)));
    tick(world, 2, 64, 2, random, changes);
    CHECK(changes.empty());
    CHECK(unrolled(random, 0));
}

TEST_CASE("a short stem climbs for free and a taller one has to roll for it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // One stem over open air: height one, which never reaches the roll.
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("chorus_plant")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);
    REQUIRE(changes.size() == 2);
    CHECK(pathAt(world, 8, 65, 8) == "chorus_flower");
    CHECK(unrolled(random, 0));

    // Two stems over open air: height two, and the first roll out of four is a two, so it just
    // makes it.
    WorldState twoTall = emptyWorld();
    twoTall.getOrGenerateChunk(0, 0);
    REQUIRE(twoTall.setBlockState(8, 62, 8, stateOf("chorus_plant")));
    REQUIRE(twoTall.setBlockState(8, 63, 8, stateOf("chorus_plant")));
    REQUIRE(twoTall.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource second = rng::RandomSource::legacy(0);
    changes.clear();
    tick(twoTall, 8, 64, 8, second, changes);
    CHECK(pathAt(twoTall, 8, 65, 8) == "chorus_flower");
    // Two is already past the free height, so it climbed on a roll and not for nothing.
    CHECK_FALSE(unrolled(second, 0));

    // Three stems, same roll of two: three is one too many and it never climbs.
    WorldState threeTall = emptyWorld();
    threeTall.getOrGenerateChunk(0, 0);
    for (std::int32_t y = 61; y <= 63; ++y) {
        REQUIRE(threeTall.setBlockState(8, y, 8, stateOf("chorus_plant")));
    }
    REQUIRE(threeTall.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource third = rng::RandomSource::legacy(0);
    changes.clear();
    tick(threeTall, 8, 64, 8, third, changes);
    CHECK(pathAt(threeTall, 8, 65, 8) == "air");
}

TEST_CASE("a stem standing on end stone rolls out of five and gets an extra branch attempt") {
    // Four stems on end stone, which is as deep as the stem is ever measured: seed three rolls a
    // four out of five, which four still clears.
    WorldState lucky = emptyWorld();
    lucky.getOrGenerateChunk(0, 0);
    REQUIRE(lucky.setBlockState(8, 59, 8, stateOf("end_stone")));
    for (std::int32_t y = 60; y <= 63; ++y) {
        REQUIRE(lucky.setBlockState(8, y, 8, stateOf("chorus_plant")));
    }
    REQUIRE(lucky.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource random = rng::RandomSource::legacy(3);
    std::vector<BlockChange> changes;
    tick(lucky, 8, 64, 8, random, changes);
    REQUIRE(changes.size() == 2);
    CHECK(pathAt(lucky, 8, 65, 8) == "chorus_flower");

    // Same shape, seed zero: a zero out of five, so it branches instead, and standing on end stone
    // buys it a fourth attempt out of the three it rolled.
    WorldState unlucky = emptyWorld();
    unlucky.getOrGenerateChunk(0, 0);
    REQUIRE(unlucky.setBlockState(8, 59, 8, stateOf("end_stone")));
    for (std::int32_t y = 60; y <= 63; ++y) {
        REQUIRE(unlucky.setBlockState(8, y, 8, stateOf("chorus_plant")));
    }
    REQUIRE(unlucky.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource second = rng::RandomSource::legacy(0);
    changes.clear();
    tick(unlucky, 8, 64, 8, second, changes);

    // North, south, south again and east: the repeat finds its own flower and is dropped.
    REQUIRE(changes.size() == 4);
    CHECK(pathAt(unlucky, 8, 64, 7) == "chorus_flower");
    CHECK(pathAt(unlucky, 8, 64, 9) == "chorus_flower");
    CHECK(pathAt(unlucky, 9, 64, 8) == "chorus_flower");
    CHECK(pathAt(unlucky, 8, 64, 8) == "chorus_plant");
    // Without the extra attempt the fourth roll never happens and the east side stays empty.
    CHECK(ageAt(unlucky, 9, 64, 8) == "1");
}

TEST_CASE("a flower that cannot climb branches sideways and ages as it goes") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(3)));
    // Two above is in the way, so there is no room to climb even standing on ground.
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("stone")));

    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);

    // Two attempts, north then east, both clear.
    REQUIRE(changes.size() == 3);
    CHECK(pathAt(world, 8, 64, 7) == "chorus_flower");
    CHECK(pathAt(world, 9, 64, 8) == "chorus_flower");
    CHECK(pathAt(world, 8, 64, 8) == "chorus_plant");
    // parity: a branch is one age older than the flower it came from.
    CHECK(ageAt(world, 8, 64, 7) == "4");
    CHECK(ageAt(world, 9, 64, 8) == "4");
    // parity: `allNeighborsEmpty` lets the side the branch came from through, otherwise the second
    // branch would see the first flower's own stem and refuse.
    CHECK(pathAt(world, 8, 65, 8) == "air");
}

TEST_CASE("the same side is only taken once") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(0)));
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("stone")));

    // Seed five rolls north twice out of its two attempts.
    rng::RandomSource random = rng::RandomSource::legacy(5);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);

    REQUIRE(changes.size() == 2);
    CHECK(pathAt(world, 8, 64, 7) == "chorus_flower");
    CHECK(pathAt(world, 8, 64, 8) == "chorus_plant");
    CHECK(pathAt(world, 9, 64, 8) == "air");
}

TEST_CASE("a branch refuses a spot that is crowded or unsupported") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(0)));
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("stone")));
    // North is blocked from below, east has a neighbour of its own two blocks away.
    REQUIRE(world.setBlockState(8, 63, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("stone")));

    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);

    // Neither attempt lands, so the flower dies where it stands.
    REQUIRE(changes.size() == 1);
    CHECK(pathAt(world, 8, 64, 7) == "air");
    CHECK(pathAt(world, 9, 64, 8) == "air");
    CHECK(pathAt(world, 8, 64, 8) == "chorus_flower");
    CHECK(ageAt(world, 8, 64, 8) == "5");
}

TEST_CASE("an old flower dies instead of branching") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(chorusFlowerGrowth::kBranchAge)));
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("stone")));

    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);

    // Every side is free, so an age three flower here would have branched.
    REQUIRE(changes.size() == 1);
    CHECK(pathAt(world, 8, 64, 7) == "air");
    CHECK(pathAt(world, 9, 64, 8) == "air");
    CHECK(ageAt(world, 8, 64, 8) == "5");
    CHECK(unrolled(random, 1));
}

TEST_CASE("a flower over open air climbs and one over stone does not") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);
    REQUIRE(changes.size() == 2);
    CHECK(pathAt(world, 8, 65, 8) == "chorus_flower");
    CHECK(unrolled(random, 0));

    // Stone is not `#supports_chorus_flower`, so a flower on it never climbs however clear the sky
    // over it is.
    WorldState onStone = emptyWorld();
    onStone.getOrGenerateChunk(0, 0);
    REQUIRE(onStone.setBlockState(8, 63, 8, stateOf("stone")));
    REQUIRE(onStone.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource second = rng::RandomSource::legacy(0);
    changes.clear();
    tick(onStone, 8, 64, 8, second, changes);
    CHECK(pathAt(onStone, 8, 65, 8) == "air");
}

TEST_CASE("a flower with a neighbour beside its head cannot climb past it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(0)));
    // Beside the block it would move into, which is what `allNeighborsEmpty` is asked about.
    REQUIRE(world.setBlockState(9, 65, 8, stateOf("stone")));

    rng::RandomSource random = rng::RandomSource::legacy(1);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);

    CHECK(pathAt(world, 8, 65, 8) == "air");
    // It branched instead, so the sides it rolled are what changed.
    REQUIRE(changes.size() == 3);
    CHECK(pathAt(world, 8, 64, 7) == "chorus_flower");
    CHECK(pathAt(world, 9, 64, 8) == "chorus_flower");
}

TEST_CASE("a fifth stem down is out of reach of the measurement") {
    // Five stems on end stone: the stem is only followed four blocks, so the ground under it is
    // never found and the flower neither rolls out of five nor earns the extra attempt.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 58, 8, stateOf("end_stone")));
    for (std::int32_t y = 59; y <= 63; ++y) {
        REQUIRE(world.setBlockState(8, y, 8, stateOf("chorus_plant")));
    }
    REQUIRE(world.setBlockState(8, 64, 8, flowerOf(0)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    tick(world, 8, 64, 8, random, changes);

    // Five is over any roll out of four, so it branches: north, south and south again, and no
    // more. A fourth attempt would have gone east, which is what finding the ground would buy it.
    REQUIRE(changes.size() == 3);
    CHECK(pathAt(world, 8, 65, 8) == "air");
    CHECK(pathAt(world, 8, 64, 7) == "chorus_flower");
    CHECK(pathAt(world, 8, 64, 9) == "chorus_flower");
    CHECK(pathAt(world, 9, 64, 8) == "air");
}

TEST_CASE("the bottom of the world is not ground under a stem") {
    // parity: vanilla answers void air for a position under the world, which is neither the plant
    // nor anything the tag holds, so the stem is not standing on a support block.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, -64, 8, stateOf("chorus_plant")));
    REQUIRE(world.setBlockState(8, -63, 8, stateOf("chorus_plant")));
    REQUIRE(world.setBlockState(8, -62, 8, flowerOf(0)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    tick(world, 8, -62, 8, random, changes);

    // Two out of four is a two, so it climbs. Out of five it would have been a zero and branched.
    REQUIRE(changes.size() == 2);
    CHECK(pathAt(world, 8, -61, 8) == "chorus_flower");
    CHECK(pathAt(world, 8, -62, 8) == "chorus_plant");
}
