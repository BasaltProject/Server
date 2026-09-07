#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/RandomTicks.hpp>
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

/// A seed whose first roll of `bound` comes up zero while the rolls one either side do not.
///
/// Derived rather than written down: a test that acts on this seed says the bound is exactly this
/// one, and a build that rolled against a bound one larger or smaller would not act at all.
[[nodiscard]] std::int64_t seedRollingZeroOnlyAt(std::int32_t bound) {
    for (std::int64_t seed = 1; seed < 200000; seed++) {
        rng::RandomSource exact = rng::RandomSource::legacy(seed);
        rng::RandomSource smaller = rng::RandomSource::legacy(seed);
        rng::RandomSource larger = rng::RandomSource::legacy(seed);
        if (exact.nextInt(bound) == 0 && smaller.nextInt(bound - 1) != 0 && larger.nextInt(bound + 1) != 0) {
            return seed;
        }
    }
    REQUIRE_MESSAGE(false, "no seed rolls zero at this bound alone");
    return 0;
}

/// A seed whose first double falls between `low` and `high`, so an odds check either side of it
/// answers differently.
[[nodiscard]] std::int64_t seedRollingBetween(double low, double high) {
    for (std::int64_t seed = 1; seed < 200000; seed++) {
        rng::RandomSource random = rng::RandomSource::legacy(seed);
        const double rolled = random.nextDouble();
        if (rolled > low && rolled <= high) {
            return seed;
        }
    }
    REQUIRE_MESSAGE(false, "no seed rolls a double in this range");
    return 0;
}

/// A world and the rules and clock a context borrows, kept together because a context holds
/// references to all three and `WorldState` neither copies nor moves.
struct Loaded {
    WorldState world{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
    GameRuleValues gameRules;
    WorldClock clock;

    [[nodiscard]] randomTicks::Context context() {
        return randomTicks::Context{.world = world, .worldName = "overworld", .gameRules = gameRules, .clock = clock};
    }
};

/// Lights a spot to exactly `level` with a glowstone `15 - level` blocks away, and says so.
///
/// A sky would drown the difference, so every test that uses this turns the sky off: what is being
/// measured is the threshold, not where the light came from.
void lightTo(Loaded& loaded, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) {
    const std::int32_t lamp = x + (worldLight::kMaxLevel - level);
    loaded.world.getOrGenerateChunk(lamp >> 4, z >> 4);
    REQUIRE(loaded.world.setBlockState(lamp, y, z, stateOf("glowstone")));
    REQUIRE(worldLight::rawBrightness(loaded.world, x, y, z, false) == level);
}

} // namespace

TEST_CASE("a crop grows a stage when the light and the roll are with it") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "2"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;

    // parity: `CropBlock.randomTick`'s `nextInt((int)(25.0F / speed) + 1)`, which over moist
    // farmland and nothing else is a roll in seven.
    REQUIRE(randomTicks::growthSpeed(loaded.world, 8, 65, 8, basalt::wire::common::Identifier::vanilla("wheat")) ==
            doctest::Approx(4.0F));
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));
    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 65, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(propertyOf(changes[0].stateId, "age") == "3");
    CHECK(propertyOf(*loaded.world.blockState(8, 65, 8), "age") == "3");
}

TEST_CASE("a crop on plain dirt rolls against the whole twenty-five") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("dirt")));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;

    // Nothing helps it along, so the speed is one and the roll is one in twenty-six.
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(26));
    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 65, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(propertyOf(changes[0].stateId, "age") == "1");
}

TEST_CASE("a crop grows into its last age and no further") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "6"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));

    const std::vector<BlockChange> last = ticks.tickBlock(context, 8, 65, 8, random);
    REQUIRE(last.size() == 1);
    CHECK(propertyOf(last[0].stateId, "age") == "7");

    for (std::int32_t attempt = 0; attempt < 64; attempt++) {
        CHECK(ticks.tickBlock(context, 8, 65, 8, random).empty());
    }
    CHECK(propertyOf(*loaded.world.blockState(8, 65, 8), "age") == "7");
}

TEST_CASE("a crop grows at light nine and not at eight") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));
    randomTicks::Context context = loaded.context();
    context.hasSkylight = false;
    RandomTicks ticks;

    lightTo(loaded, 8, 65, 8, 8);
    rng::RandomSource dark = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));
    CHECK(ticks.tickBlock(context, 8, 65, 8, dark).empty());

    Loaded brighter;
    brighter.world.getOrGenerateChunk(0, 0);
    REQUIRE(brighter.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(brighter.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));
    randomTicks::Context lit = brighter.context();
    lit.hasSkylight = false;
    lightTo(brighter, 8, 65, 8, 9);
    rng::RandomSource bright = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));
    CHECK(ticks.tickBlock(lit, 8, 65, 8, bright).size() == 1);
}

TEST_CASE("a beetroot throws away one turn in three before it asks to grow at all") {
    // parity: `BeetrootBlock.randomTick`'s own `nextInt(3)`, taken from the same random the growth
    // roll comes from. The seed is picked so that the turn is thrown away at a bound of three and
    // kept at either of the bounds beside it, and the growth roll that follows would land: a
    // beetroot that stays as it is says the bound is three and not two or four.
    std::int64_t seed = 1;
    for (; seed < 200000; seed++) {
        rng::RandomSource three = rng::RandomSource::legacy(seed);
        rng::RandomSource two = rng::RandomSource::legacy(seed);
        rng::RandomSource four = rng::RandomSource::legacy(seed);
        const bool skips = three.nextInt(3) == 0;
        const bool growsAtTwo = two.nextInt(2) != 0 && two.nextInt(7) == 0;
        const bool growsAtFour = four.nextInt(4) != 0 && four.nextInt(7) == 0;
        if (skips && growsAtTwo && growsAtFour) {
            break;
        }
    }
    REQUIRE(seed < 200000);

    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("beetroots", {{"age", "0"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seed);

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).empty());
}

TEST_CASE("a beetroot that keeps its turn grows like any other crop") {
    // The other side of the same roll: a turn that is not thrown away reaches the growth roll.
    std::int64_t seed = 1;
    for (; seed < 200000; seed++) {
        rng::RandomSource random = rng::RandomSource::legacy(seed);
        if (random.nextInt(3) != 0 && random.nextInt(7) == 0) {
            break;
        }
    }
    REQUIRE(seed < 200000);

    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("beetroots", {{"age", "0"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seed);

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 65, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(propertyOf(changes[0].stateId, "age") == "1");
}

TEST_CASE("a wheat does not throw a turn away at all") {
    // The same seed a beetroot would have skipped on: wheat spends it on the growth roll instead.
    std::int64_t seed = 1;
    for (; seed < 200000; seed++) {
        rng::RandomSource random = rng::RandomSource::legacy(seed);
        if (random.nextInt(3) == 0 && rng::RandomSource::legacy(seed).nextInt(7) == 0) {
            break;
        }
    }
    REQUIRE(seed < 200000);

    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seed);

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).size() == 1);
}

TEST_CASE("moist farmland under a crop and its neighbours decide how fast it grows") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    const basalt::wire::common::Identifier wheat = basalt::wire::common::Identifier::vanilla("wheat");
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));

    // Nothing underneath: the base speed and nothing else.
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(1.0F));

    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "0"}})));
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(2.0F));

    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(4.0F));

    // A neighbour on the same row is a quarter of a block of farmland.
    REQUIRE(loaded.world.setBlockState(9, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(4.75F));

    // Two blocks out is out of the ring altogether.
    REQUIRE(loaded.world.setBlockState(10, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(4.75F));

    // parity: a crop with the same crop on a diagonal grows at half the speed.
    REQUIRE(loaded.world.setBlockState(9, 65, 9, stateOf("wheat", {{"age", "0"}})));
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(4.75F / 2.0F));
}

TEST_CASE("a crop between two rows of its own kind grows at half speed") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    const basalt::wire::common::Identifier wheat = basalt::wire::common::Identifier::vanilla("wheat");
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));

    // parity: one axis alone is a row, and a row does not halve anything.
    REQUIRE(loaded.world.setBlockState(7, 65, 8, stateOf("wheat", {{"age", "0"}})));
    REQUIRE(loaded.world.setBlockState(9, 65, 8, stateOf("wheat", {{"age", "0"}})));
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(1.0F));

    REQUIRE(loaded.world.setBlockState(8, 65, 7, stateOf("wheat", {{"age", "0"}})));
    CHECK(randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat) == doctest::Approx(0.5F));
}

TEST_CASE("a pitcher crop grows its upper half when it reaches the age that needs one") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("pitcher_crop", {{"age", "2"}, {"half", "lower"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 65, 8, random);

    REQUIRE(changes.size() == 2);
    CHECK(propertyOf(changes[0].stateId, "half") == "lower");
    CHECK(propertyOf(changes[0].stateId, "age") == "3");
    CHECK(propertyOf(changes[1].stateId, "half") == "upper");
    CHECK(changes[1].y == 66);
    CHECK(propertyOf(*loaded.world.blockState(8, 66, 8), "age") == "3");
}

TEST_CASE("a pitcher crop below the age that needs a second block grows only itself") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("pitcher_crop", {{"age", "1"}, {"half", "lower"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 65, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(propertyOf(changes[0].stateId, "age") == "2");
    CHECK(*loaded.world.blockState(8, 66, 8) == registry::blocks::air().stateId);
}

TEST_CASE("a pitcher crop at its last age is done") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("pitcher_crop", {{"age", "4"}, {"half", "lower"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).empty());
}

TEST_CASE("a pitcher crop grows at light eight, one below what a crop needs") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("pitcher_crop", {{"age", "0"}, {"half", "lower"}})));
    randomTicks::Context context = loaded.context();
    context.hasSkylight = false;
    lightTo(loaded, 8, 65, 8, 8);
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).size() == 1);

    Loaded darker;
    darker.world.getOrGenerateChunk(0, 0);
    REQUIRE(darker.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(darker.world.setBlockState(8, 65, 8, stateOf("pitcher_crop", {{"age", "0"}, {"half", "lower"}})));
    randomTicks::Context dark = darker.context();
    dark.hasSkylight = false;
    lightTo(darker, 8, 65, 8, 7);
    rng::RandomSource other = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));
    CHECK(ticks.tickBlock(dark, 8, 65, 8, other).empty());
}

TEST_CASE("a pitcher crop with a block over it stays short") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("pitcher_crop", {{"age", "2"}, {"half", "lower"}})));
    REQUIRE(loaded.world.setBlockState(8, 66, 8, stateOf("stone")));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).empty());
    CHECK(pathOf(*loaded.world.blockState(8, 66, 8)) == "stone");
}

TEST_CASE("a pitcher crop at the top of the world has nowhere to put its other half") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "7"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("pitcher_crop", {{"age", "2"}, {"half", "lower"}})));
    randomTicks::Context context = loaded.context();
    context.maxY = 65;
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).empty());

    // One block higher and there is room, so the same crop grows.
    context.maxY = 66;
    rng::RandomSource other = rng::RandomSource::legacy(seedRollingZeroOnlyAt(7));
    CHECK(ticks.tickBlock(context, 8, 65, 8, other).size() == 2);
}

TEST_CASE("nether wart grows in the dark on one roll in ten") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("soul_sand")));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("nether_wart", {{"age", "1"}})));
    randomTicks::Context context = loaded.context();
    context.hasSkylight = false;
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(10));

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 65, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(propertyOf(changes[0].stateId, "age") == "2");
}

TEST_CASE("nether wart stops at its third age") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("soul_sand")));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("nether_wart", {{"age", "2"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingZeroOnlyAt(10));

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).size() == 1);
    CHECK(propertyOf(*loaded.world.blockState(8, 65, 8), "age") == "3");

    for (std::int32_t attempt = 0; attempt < 256; attempt++) {
        CHECK(ticks.tickBlock(context, 8, 65, 8, random).empty());
    }
}

TEST_CASE("sugar cane ages until it puts a fresh block on top of itself") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("sugar_cane", {{"age", "3"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<BlockChange> aged = ticks.tickBlock(context, 8, 64, 8, random);
    REQUIRE(aged.size() == 1);
    CHECK(propertyOf(aged[0].stateId, "age") == "4");

    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("sugar_cane", {{"age", "14"}})));
    const std::vector<BlockChange> older = ticks.tickBlock(context, 8, 64, 8, random);
    REQUIRE(older.size() == 1);
    CHECK(propertyOf(older[0].stateId, "age") == "15");
    CHECK(*loaded.world.blockState(8, 65, 8) == registry::blocks::air().stateId);

    const std::vector<BlockChange> grown = ticks.tickBlock(context, 8, 64, 8, random);
    REQUIRE(grown.size() == 2);
    CHECK(grown[0].y == 65);
    CHECK(propertyOf(grown[0].stateId, "age") == "0");
    CHECK(propertyOf(*loaded.world.blockState(8, 64, 8), "age") == "0");
}

TEST_CASE("sugar cane with anything over it does nothing") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("sugar_cane", {{"age", "15"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("stone")));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
}

TEST_CASE("sugar cane two blocks tall still grows, and three does not") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("sugar_cane", {{"age", "15"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("sugar_cane", {{"age", "15"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(ticks.tickBlock(context, 8, 65, 8, random).size() == 2);

    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("sugar_cane", {{"age", "15"}})));
    REQUIRE(loaded.world.setBlockState(8, 66, 8, stateOf("sugar_cane", {{"age", "15"}})));
    CHECK(ticks.tickBlock(context, 8, 66, 8, random).empty());
}

TEST_CASE("a cactus at its last age and its full height does nothing at all") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 63, 8, stateOf("sand")));
    for (std::int32_t y = 64; y <= 66; y++) {
        REQUIRE(loaded.world.setBlockState(8, y, 8, stateOf("cactus", {{"age", "15"}})));
    }
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(ticks.tickBlock(context, 8, 66, 8, random).empty());

    // One block shorter and the same cactus starts a new block instead.
    REQUIRE(loaded.world.setBlockState(8, 66, 8, registry::blocks::air().stateId));
    CHECK(ticks.tickBlock(context, 8, 65, 8, random).size() == 2);
}

TEST_CASE("a short cactus flowers on a roll under a tenth, and a tall one under a quarter") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 63, 8, stateOf("sand")));
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("cactus", {{"age", "8"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;

    // parity: `CactusBlock.randomTick`'s own 0.1 and 0.25. A roll between the two flowers a tall
    // cactus and not a short one, which is what says the two numbers are different.
    rng::RandomSource between = rng::RandomSource::legacy(seedRollingBetween(0.1, 0.25));
    const std::vector<BlockChange> short_ = ticks.tickBlock(context, 8, 64, 8, between);
    REQUIRE(short_.size() == 1);
    CHECK(pathOf(short_[0].stateId) == "cactus");
    CHECK(propertyOf(short_[0].stateId, "age") == "9");

    Loaded tall;
    tall.world.getOrGenerateChunk(0, 0);
    REQUIRE(tall.world.setBlockState(8, 63, 8, stateOf("sand")));
    for (std::int32_t y = 64; y <= 66; y++) {
        REQUIRE(tall.world.setBlockState(8, y, 8, stateOf("cactus", {{"age", "8"}})));
    }
    randomTicks::Context tallContext = tall.context();
    rng::RandomSource same = rng::RandomSource::legacy(seedRollingBetween(0.1, 0.25));
    const std::vector<BlockChange> flowered = ticks.tickBlock(tallContext, 8, 66, 8, same);

    REQUIRE(flowered.size() == 2);
    CHECK(pathOf(flowered[0].stateId) == "cactus_flower");
    CHECK(flowered[0].y == 67);
    CHECK(propertyOf(flowered[1].stateId, "age") == "9");
}

TEST_CASE("a cactus under a tenth flowers however short it is") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 63, 8, stateOf("sand")));
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("cactus", {{"age", "8"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingBetween(0.0, 0.1));

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(changes.size() == 2);
    CHECK(pathOf(changes[0].stateId) == "cactus_flower");
    CHECK(changes[0].y == 65);
}

TEST_CASE("a cactus with a solid block beside where the flower would go grows nothing there") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 63, 8, stateOf("sand")));
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("cactus", {{"age", "8"}})));
    REQUIRE(loaded.world.setBlockState(9, 65, 8, stateOf("stone")));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingBetween(0.0, 0.1));

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(pathOf(changes[0].stateId) == "cactus");
}

TEST_CASE("grass under something opaque goes back to dirt") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "false"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("stone")));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(pathOf(changes[0].stateId) == "dirt");
}

TEST_CASE("water over grass drowns it, and a single snow layer does not") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "false"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, registry::blocks::water().stateId));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    REQUIRE(ticks.tickBlock(context, 8, 64, 8, random).size() == 1);
    CHECK(pathOf(*loaded.world.blockState(8, 64, 8)) == "dirt");

    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "true"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("snow", {{"layers", "1"}})));
    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
    CHECK(pathOf(*loaded.world.blockState(8, 64, 8)) == "grass_block");

    // parity: only the one layer is spared. Two of them let as much light through, but a whole
    // block of snow does not, and grass under that dies like grass under anything else.
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("snow", {{"layers", "2"}})));
    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("snow", {{"layers", "8"}})));
    REQUIRE(ticks.tickBlock(context, 8, 64, 8, random).size() == 1);
    CHECK(pathOf(*loaded.world.blockState(8, 64, 8)) == "dirt");
}

TEST_CASE("grass spreads at light nine and not at eight, and takes its four turns either way") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    for (std::int32_t dx = -1; dx <= 1; dx++) {
        for (std::int32_t dz = -1; dz <= 1; dz++) {
            REQUIRE(loaded.world.setBlockState(8 + dx, 64, 8 + dz, stateOf("dirt")));
        }
    }
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "false"}})));
    randomTicks::Context context = loaded.context();
    context.hasSkylight = false;
    lightTo(loaded, 8, 65, 8, 8);
    RandomTicks ticks;
    rng::RandomSource dark = rng::RandomSource::legacy(5);

    for (std::int32_t turn = 0; turn < 64; turn++) {
        CHECK(ticks.tickBlock(context, 8, 64, 8, dark).empty());
    }

    Loaded lit;
    lit.world.getOrGenerateChunk(0, 0);
    for (std::int32_t dx = -1; dx <= 1; dx++) {
        for (std::int32_t dz = -1; dz <= 1; dz++) {
            REQUIRE(lit.world.setBlockState(8 + dx, 64, 8 + dz, stateOf("dirt")));
        }
    }
    REQUIRE(lit.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "false"}})));
    randomTicks::Context bright = lit.context();
    bright.hasSkylight = false;
    lightTo(lit, 8, 65, 8, 9);
    rng::RandomSource counted = rng::RandomSource::legacy(5);
    (void)ticks.tickBlock(bright, 8, 64, 8, counted);

    // parity: four attempts of three draws each, however many of them land anywhere. The middle
    // draw of the three is the one over five, which is what puts the target within three below.
    rng::RandomSource same = rng::RandomSource::legacy(5);
    for (std::int32_t attempt = 0; attempt < 4; attempt++) {
        (void)same.nextInt(3);
        (void)same.nextInt(5);
        (void)same.nextInt(3);
    }
    CHECK(same.nextInt(1000) == counted.nextInt(1000));

    std::int32_t spread = 0;
    for (std::int32_t turn = 0; turn < 64 && spread == 0; turn++) {
        for (const BlockChange& change : ticks.tickBlock(bright, 8, 64, 8, counted)) {
            CHECK(pathOf(change.stateId) == "grass_block");
            spread++;
        }
    }
    CHECK(spread > 0);
}

TEST_CASE("grass does not spread under water, nor onto anything but plain dirt") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "false"}})));
    // Coarse dirt and podzol are dirt to look at and not the block grass spreads onto.
    for (std::int32_t dx = -1; dx <= 1; dx++) {
        for (std::int32_t dz = -1; dz <= 1; dz++) {
            REQUIRE(loaded.world.setBlockState(8 + dx, 64, 8 + dz, stateOf("coarse_dirt")));
        }
    }
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "false"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(5);

    for (std::int32_t turn = 0; turn < 64; turn++) {
        CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
    }

    // Plain dirt with water over it is out too.
    REQUIRE(loaded.world.setBlockState(9, 64, 8, stateOf("dirt")));
    REQUIRE(loaded.world.setBlockState(9, 65, 8, registry::blocks::water().stateId));
    for (std::int32_t turn = 0; turn < 64; turn++) {
        CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
    }
}

TEST_CASE("farmland dries a step at a time and then goes back to dirt") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "2"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(propertyOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId, "moisture") == "1");
    CHECK(propertyOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId, "moisture") == "0");
    CHECK(pathOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId) == "dirt");
}

TEST_CASE("a crop growing on farmland holds it together at nothing left") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "0"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
    CHECK(pathOf(*loaded.world.blockState(8, 64, 8)) == "farmland");
}

TEST_CASE("rain wets farmland however far the water is") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "0"}})));
    randomTicks::Context context = loaded.context();
    context.raining = true;
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(propertyOf(changes[0].stateId, "moisture") == "7");

    // Already as wet as it goes: nothing to write.
    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
}

TEST_CASE("water four blocks out wets farmland, and five does not") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "1"}})));
    REQUIRE(loaded.world.setBlockState(13, 64, 8, registry::blocks::water().stateId));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // Five blocks out is out of reach, so it dries instead of filling up.
    CHECK(propertyOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId, "moisture") == "0");

    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "1"}})));
    REQUIRE(loaded.world.setBlockState(12, 64, 8, registry::blocks::water().stateId));
    CHECK(propertyOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId, "moisture") == "7");
}

TEST_CASE("water one block over farmland counts, and two does not") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "1"}})));
    REQUIRE(loaded.world.setBlockState(10, 66, 8, registry::blocks::water().stateId));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(propertyOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId, "moisture") == "0");

    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "1"}})));
    REQUIRE(loaded.world.setBlockState(10, 65, 8, registry::blocks::water().stateId));
    CHECK(propertyOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId, "moisture") == "7");
}

TEST_CASE("leaves seven blocks from a trunk decay, and persistent ones never do") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(
        8, 64, 8, stateOf("oak_leaves", {{"distance", "7"}, {"persistent", "false"}, {"waterlogged", "false"}})));
    REQUIRE(loaded.world.setBlockState(
        9, 64, 8, stateOf("oak_leaves", {{"distance", "6"}, {"persistent", "false"}, {"waterlogged", "false"}})));
    REQUIRE(loaded.world.setBlockState(
        10, 64, 8, stateOf("oak_leaves", {{"distance", "7"}, {"persistent", "true"}, {"waterlogged", "false"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<BlockChange> gone = ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(gone.size() == 1);
    CHECK(gone[0].stateId == registry::blocks::air().stateId);
    CHECK(ticks.tickBlock(context, 9, 64, 8, random).empty());
    CHECK(ticks.tickBlock(context, 10, 64, 8, random).empty());
}

TEST_CASE("ice melts into water over the light it takes, and into nothing where water boils") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("ice")));
    randomTicks::Context context = loaded.context();
    context.hasSkylight = false;
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // parity: `IceBlock.randomTick`'s `> 11 - lightDampening`, so what ice itself dampens counts
    // and the light it takes is worked out from the registry rather than written down.
    const std::int32_t dampens = registry::blockShapes::dampening(stateOf("ice"));
    lightTo(loaded, 8, 64, 8, 11 - dampens);
    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());

    Loaded brighter;
    brighter.world.getOrGenerateChunk(0, 0);
    REQUIRE(brighter.world.setBlockState(8, 64, 8, stateOf("ice")));
    randomTicks::Context lit = brighter.context();
    lit.hasSkylight = false;
    lightTo(brighter, 8, 64, 8, 12 - dampens);
    const std::vector<BlockChange> melted = ticks.tickBlock(lit, 8, 64, 8, random);
    REQUIRE(melted.size() == 1);
    CHECK(pathOf(melted[0].stateId) == "water");

    // parity: `IceBlock.melt`, which removes the block where the dimension boils water away.
    Loaded nether;
    nether.world.getOrGenerateChunk(0, 0);
    REQUIRE(nether.world.setBlockState(8, 64, 8, stateOf("ice")));
    randomTicks::Context boiling = nether.context();
    boiling.hasSkylight = false;
    boiling.evaporates = true;
    lightTo(nether, 8, 64, 8, 12 - dampens);
    const std::vector<BlockChange> gone = ticks.tickBlock(boiling, 8, 64, 8, random);
    REQUIRE(gone.size() == 1);
    CHECK(gone[0].stateId == registry::blocks::air().stateId);
}

TEST_CASE("snow melts away over the same light and leaves nothing behind") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("snow", {{"layers", "3"}})));
    randomTicks::Context context = loaded.context();
    context.hasSkylight = false;
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    lightTo(loaded, 8, 64, 8, 11);
    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());

    Loaded brighter;
    brighter.world.getOrGenerateChunk(0, 0);
    REQUIRE(brighter.world.setBlockState(8, 64, 8, stateOf("snow", {{"layers", "3"}})));
    randomTicks::Context lit = brighter.context();
    lit.hasSkylight = false;
    lightTo(brighter, 8, 64, 8, 12);
    const std::vector<BlockChange> melted = ticks.tickBlock(lit, 8, 64, 8, random);

    REQUIRE(melted.size() == 1);
    CHECK(melted[0].stateId == registry::blocks::air().stateId);
}

TEST_CASE("a change a plugin refuses leaves the block where it was") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "2"}})));
    randomTicks::Context context = loaded.context();
    std::vector<std::string> causes;
    context.mayChange = [&causes](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                  std::string_view cause) {
        causes.emplace_back(cause);
        return false;
    };
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
    CHECK(propertyOf(*loaded.world.blockState(8, 64, 8), "moisture") == "2");
    REQUIRE(causes.size() == 1);
    CHECK(causes[0] == "grow");
}

TEST_CASE("a block taken away is a fade and one moving on is a growth") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("ice")));
    randomTicks::Context context = loaded.context();
    context.hasSkylight = false;
    lightTo(loaded, 8, 64, 8, 12);
    REQUIRE(loaded.world.setBlockState(
        8, 70, 8, stateOf("oak_leaves", {{"distance", "7"}, {"persistent", "false"}, {"waterlogged", "false"}})));
    REQUIRE(loaded.world.setBlockState(11, 70, 8, stateOf("farmland", {{"moisture", "3"}})));
    std::vector<std::string> causes;
    context.mayChange = [&causes](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                  std::string_view cause) {
        causes.emplace_back(cause);
        return true;
    };
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    (void)ticks.tickBlock(context, 8, 64, 8, random);
    (void)ticks.tickBlock(context, 8, 70, 8, random);
    (void)ticks.tickBlock(context, 11, 70, 8, random);

    REQUIRE(causes.size() == 3);
    // Ice into water is one block moving on to another, and vanilla calls it a fade all the same.
    CHECK(causes[0] == "fade");
    CHECK(causes[1] == "fade");
    CHECK(causes[2] == "grow");
}

TEST_CASE("a sweep at speed zero picks nothing at all") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "3"}})));
    randomTicks::Context context = loaded.context();
    context.seed = 12345;
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);
    const std::array<std::array<std::int32_t, 2>, 1> chunks{{{0, 0}}};

    CHECK(ticks.tick(context, chunks, 0, random).empty());
    CHECK(ticks.tick(context, chunks, -1, random).empty());
}

TEST_CASE("a sweep over a chunk that is not loaded picks nothing") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);
    const std::array<std::array<std::int32_t, 2>, 1> chunks{{{40, 40}}};

    CHECK(ticks.tick(context, chunks, 3, random).empty());
}

TEST_CASE("the walk decides which positions come up, and one section's worth is one step each") {
    // parity: `Level.getBlockRandomPos`. Working the walk out here rather than recording where it
    // landed says the multiplier, the addend and the three shifts are all what vanilla's are.
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    for (std::int32_t x = 0; x < 16; x++) {
        for (std::int32_t z = 0; z < 16; z++) {
            for (std::int32_t y = 64; y < 80; y++) {
                REQUIRE(loaded.world.setBlockState(x, y, z, stateOf("farmland", {{"moisture", "3"}})));
            }
        }
    }
    randomTicks::Context context = loaded.context();
    context.seed = 987654321;
    const std::array<std::array<std::int32_t, 2>, 1> chunks{{{0, 0}}};
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(2);

    const std::vector<BlockChange> changes = ticks.tick(context, chunks, 3, random);

    // Only the one section holds anything that ticks, so the walk takes exactly three steps.
    REQUIRE(changes.size() == 3);
    std::uint32_t walk = static_cast<std::uint32_t>(context.seed);
    for (const BlockChange& change : changes) {
        walk = (walk * static_cast<std::uint32_t>(randomTicks::kWalkMultiplier)) +
               static_cast<std::uint32_t>(randomTicks::kWalkAddend);
        const std::int32_t picked = static_cast<std::int32_t>(walk) >> 2;
        CHECK(change.x == (picked & 15));
        CHECK(change.y == 64 + ((picked >> 16) & 15));
        CHECK(change.z == ((picked >> 8) & 15));
        CHECK(propertyOf(change.stateId, "moisture") == "2");
    }
}

TEST_CASE("the walk starts where the world's seed says and carries on from there") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    for (std::int32_t x = 0; x < 16; x++) {
        for (std::int32_t z = 0; z < 16; z++) {
            for (std::int32_t y = 64; y < 80; y++) {
                REQUIRE(loaded.world.setBlockState(x, y, z, stateOf("farmland", {{"moisture", "3"}})));
            }
        }
    }
    randomTicks::Context context = loaded.context();
    context.seed = 555;
    const std::array<std::array<std::int32_t, 2>, 1> chunks{{{0, 0}}};
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(2);

    const std::vector<BlockChange> first = ticks.tick(context, chunks, 2, random);
    const std::vector<BlockChange> second = ticks.tick(context, chunks, 2, random);
    REQUIRE(first.size() == 2);
    REQUIRE(second.size() == 2);
    const bool moved = first[0].x != second[0].x || first[0].y != second[0].y || first[0].z != second[0].z;
    CHECK(moved);

    // Another world with the same seed starts its own walk, so it picks what the first world did.
    randomTicks::Context elsewhere = loaded.context();
    elsewhere.worldName = "the_nether";
    elsewhere.seed = context.seed;
    const std::vector<BlockChange> other = ticks.tick(elsewhere, chunks, 2, random);
    REQUIRE(other.size() == 2);
    CHECK(other[0].x == first[0].x);
    CHECK(other[0].y == first[0].y);
    CHECK(other[0].z == first[0].z);

    // A different seed puts the same world somewhere else. Which seed that is has to be worked
    // out: the walk keeps twelve bits of its value, so two seeds a step apart often pick the same
    // block.
    const auto pickedFor = [](std::int64_t seed) {
        const std::uint32_t walk = (static_cast<std::uint32_t>(static_cast<std::int32_t>(seed)) *
                                    static_cast<std::uint32_t>(randomTicks::kWalkMultiplier)) +
                                   static_cast<std::uint32_t>(randomTicks::kWalkAddend);
        return (static_cast<std::int32_t>(walk) >> 2) & 0x000F0F0F;
    };
    std::int64_t apart = context.seed + 1;
    while (pickedFor(apart) == pickedFor(context.seed)) {
        apart++;
    }
    randomTicks::Context afresh = loaded.context();
    afresh.worldName = "the_end";
    afresh.seed = apart;
    const std::vector<BlockChange> different = ticks.tick(afresh, chunks, 2, random);
    REQUIRE(different.size() == 2);
    const bool elsewhereStill =
        different[0].x != first[0].x || different[0].y != first[0].y || different[0].z != first[0].z;
    CHECK(elsewhereStill);
}

TEST_CASE("a section with nothing that ticks is stepped over, and the walk with it") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(3, 70, 5, stateOf("farmland", {{"moisture", "3"}})));
    randomTicks::Context context = loaded.context();
    context.seed = 42;
    const std::array<std::array<std::int32_t, 2>, 1> chunks{{{0, 0}}};
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    (void)ticks.tick(context, chunks, 8, random);

    // One section held anything, so the walk took eight steps and no more: a world seeded where
    // this one has got to picks exactly what this one picks next.
    std::uint32_t walk = static_cast<std::uint32_t>(context.seed);
    for (std::int32_t step = 0; step < 8; step++) {
        walk = (walk * static_cast<std::uint32_t>(randomTicks::kWalkMultiplier)) +
               static_cast<std::uint32_t>(randomTicks::kWalkAddend);
    }
    randomTicks::Context measured = loaded.context();
    measured.worldName = "measured";
    measured.seed = static_cast<std::int64_t>(static_cast<std::int32_t>(walk));
    RandomTicks fresh;
    const std::vector<BlockChange> next = ticks.tick(context, chunks, 1, random);
    const std::vector<BlockChange> predicted = fresh.tick(measured, chunks, 1, random);
    CHECK(next.size() == predicted.size());
    for (std::size_t index = 0; index < next.size(); index++) {
        CHECK(next[index].x == predicted[index].x);
        CHECK(next[index].y == predicted[index].y);
        CHECK(next[index].z == predicted[index].z);
    }
}

TEST_CASE("a block that does not tick at random is never asked") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("stone")));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
    // Nor is a position with no chunk under it.
    CHECK(ticks.tickBlock(context, 700, 64, 700, random).empty());
}

TEST_CASE("the families that live in their own files are reached by the block they belong to") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("warped_nylium")));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("stone")));
    REQUIRE(loaded.world.setBlockState(10, 64, 8, stateOf("redstone_ore", {{"lit", "true"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    // parity: `NyliumBlock.randomTick`, whose own file is `MiscGrowth`.
    const std::vector<BlockChange> nylium = ticks.tickBlock(context, 8, 64, 8, random);
    REQUIRE(nylium.size() == 1);
    CHECK(pathOf(nylium[0].stateId) == "netherrack");

    // parity: `RedStoneOreBlock.randomTick`, the light going out again.
    const std::vector<BlockChange> ore = ticks.tickBlock(context, 10, 64, 8, random);
    REQUIRE(ore.size() == 1);
    CHECK(propertyOf(ore[0].stateId, "lit") == "false");
}

TEST_CASE("a sweep of another chunk picks positions inside that chunk") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(1, -2);
    for (std::int32_t x = 16; x < 32; x++) {
        for (std::int32_t z = -32; z < -16; z++) {
            for (std::int32_t y = 64; y < 80; y++) {
                REQUIRE(loaded.world.setBlockState(x, y, z, stateOf("farmland", {{"moisture", "3"}})));
            }
        }
    }
    randomTicks::Context context = loaded.context();
    context.seed = 24680;
    const std::array<std::array<std::int32_t, 2>, 1> chunks{{{1, -2}}};
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(2);

    const std::vector<BlockChange> changes = ticks.tick(context, chunks, 3, random);

    REQUIRE(changes.size() == 3);
    std::uint32_t walk = static_cast<std::uint32_t>(context.seed);
    for (const BlockChange& change : changes) {
        walk = (walk * static_cast<std::uint32_t>(randomTicks::kWalkMultiplier)) +
               static_cast<std::uint32_t>(randomTicks::kWalkAddend);
        const std::int32_t picked = static_cast<std::int32_t>(walk) >> 2;
        CHECK(change.x == 16 + (picked & 15));
        CHECK(change.z == -32 + ((picked >> 8) & 15));
    }
}

TEST_CASE("a crop halves its speed for a neighbour on each axis, and for one on a diagonal") {
    // parity: `CropBlock.getGrowthSpeed`'s last two rules, one offset at a time so a reach that is
    // one block out on any of them shows up here.
    const basalt::wire::common::Identifier wheat = basalt::wire::common::Identifier::vanilla("wheat");
    const auto speedWith = [&wheat](const std::vector<std::pair<std::int32_t, std::int32_t>>& neighbours) {
        Loaded loaded;
        loaded.world.getOrGenerateChunk(0, 0);
        REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));
        for (const auto& [dx, dz] : neighbours) {
            REQUIRE(loaded.world.setBlockState(8 + dx, 65, 8 + dz, stateOf("wheat", {{"age", "0"}})));
        }
        return randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat);
    };

    // One axis alone is a row, and a row does not halve anything.
    CHECK(speedWith({{-1, 0}}) == doctest::Approx(1.0F));
    CHECK(speedWith({{1, 0}}) == doctest::Approx(1.0F));
    CHECK(speedWith({{0, -1}}) == doctest::Approx(1.0F));
    CHECK(speedWith({{0, 1}}) == doctest::Approx(1.0F));

    // Both axes halve it, whichever side each of them is on.
    CHECK(speedWith({{-1, 0}, {0, -1}}) == doctest::Approx(0.5F));
    CHECK(speedWith({{1, 0}, {0, 1}}) == doctest::Approx(0.5F));
    CHECK(speedWith({{-1, 0}, {0, 1}}) == doctest::Approx(0.5F));
    CHECK(speedWith({{1, 0}, {0, -1}}) == doctest::Approx(0.5F));

    // Each of the four diagonals halves it on its own.
    CHECK(speedWith({{-1, -1}}) == doctest::Approx(0.5F));
    CHECK(speedWith({{1, -1}}) == doctest::Approx(0.5F));
    CHECK(speedWith({{1, 1}}) == doctest::Approx(0.5F));
    CHECK(speedWith({{-1, 1}}) == doctest::Approx(0.5F));

    // Two blocks out is nobody's neighbour.
    CHECK(speedWith({{-2, 0}}) == doctest::Approx(1.0F));
    CHECK(speedWith({{0, 2}}) == doctest::Approx(1.0F));
    CHECK(speedWith({{2, 2}}) == doctest::Approx(1.0F));
}

TEST_CASE("the farmland under a crop is counted one row out on both axes") {
    const basalt::wire::common::Identifier wheat = basalt::wire::common::Identifier::vanilla("wheat");
    const auto speedWith = [&wheat](std::int32_t dx, std::int32_t dz, const std::string& moisture) {
        Loaded loaded;
        loaded.world.getOrGenerateChunk(0, 0);
        REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("wheat", {{"age", "0"}})));
        REQUIRE(loaded.world.setBlockState(8 + dx, 64, 8 + dz, stateOf("farmland", {{"moisture", moisture}})));
        return randomTicks::growthSpeed(loaded.world, 8, 65, 8, wheat);
    };

    // A quarter each for the ring, and dry farmland is worth a third of wet.
    CHECK(speedWith(1, 0, "7") == doctest::Approx(1.75F));
    CHECK(speedWith(0, 1, "7") == doctest::Approx(1.75F));
    CHECK(speedWith(-1, -1, "7") == doctest::Approx(1.75F));
    CHECK(speedWith(1, 0, "0") == doctest::Approx(1.25F));
    CHECK(speedWith(0, 1, "0") == doctest::Approx(1.25F));

    // Two rows out is outside the ring on either axis.
    CHECK(speedWith(2, 0, "7") == doctest::Approx(1.0F));
    CHECK(speedWith(0, 2, "7") == doctest::Approx(1.0F));
}

TEST_CASE("grass spreads three blocks down and one up, and one block out to each side") {
    // parity: `SpreadingSnowyBlock.randomTick`'s own `nextInt(3) - 1` and `nextInt(5) - 3`.
    const auto reaches = [](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        Loaded loaded;
        loaded.world.getOrGenerateChunk(0, 0);
        REQUIRE(loaded.world.setBlockState(8, 68, 8, stateOf("grass_block", {{"snowy", "false"}})));
        REQUIRE(loaded.world.setBlockState(8 + dx, 68 + dy, 8 + dz, stateOf("dirt")));
        randomTicks::Context context = loaded.context();
        RandomTicks ticks;
        rng::RandomSource random = rng::RandomSource::legacy(11);
        for (std::int32_t turn = 0; turn < 400; turn++) {
            for (const BlockChange& change : ticks.tickBlock(context, 8, 68, 8, random)) {
                if (change.x == 8 + dx && change.y == 68 + dy && change.z == 8 + dz) {
                    return true;
                }
            }
        }
        return false;
    };

    for (std::int32_t dy = -3; dy <= 1; dy++) {
        CHECK(reaches(1, dy, 0));
    }
    CHECK_FALSE(reaches(1, -4, 0));
    CHECK_FALSE(reaches(1, 2, 0));

    CHECK(reaches(-1, -1, 0));
    CHECK(reaches(1, -1, 0));
    CHECK(reaches(0, -1, -1));
    CHECK(reaches(0, -1, 1));
    CHECK_FALSE(reaches(-2, -1, 0));
    CHECK_FALSE(reaches(2, -1, 0));
    CHECK_FALSE(reaches(0, -1, -2));
    CHECK_FALSE(reaches(0, -1, 2));
}

TEST_CASE("grass will not spread onto dirt with water directly over it") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 68, 8, stateOf("grass_block", {{"snowy", "false"}})));
    REQUIRE(loaded.world.setBlockState(9, 67, 8, stateOf("dirt")));
    REQUIRE(loaded.world.setBlockState(9, 68, 8, registry::blocks::water().stateId));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(11);

    for (std::int32_t turn = 0; turn < 400; turn++) {
        CHECK(ticks.tickBlock(context, 8, 68, 8, random).empty());
    }
}

TEST_CASE("grass turning to dirt is a growth rather than a fade") {
    // Only what is taken away or put out counts as a fade, and dirt is neither.
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("grass_block", {{"snowy", "false"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("stone")));
    randomTicks::Context context = loaded.context();
    std::vector<std::string> causes;
    context.mayChange = [&causes](std::int32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                                  std::string_view cause) {
        causes.emplace_back(cause);
        return true;
    };
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    (void)ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(causes.size() == 1);
    CHECK(causes[0] == "grow");
}

TEST_CASE("a sapling advances a stage at light nine over it and one roll in seven") {
    const auto grows = [](std::int32_t light, std::int64_t seed) {
        Loaded loaded;
        loaded.world.getOrGenerateChunk(0, 0);
        REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("dirt")));
        REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("oak_sapling", {{"stage", "0"}})));
        randomTicks::Context context = loaded.context();
        context.hasSkylight = false;
        lightTo(loaded, 8, 66, 8, light);
        RandomTicks ticks;
        rng::RandomSource random = rng::RandomSource::legacy(seed);
        return !ticks.tickBlock(context, 8, 65, 8, random).empty();
    };

    const std::int64_t lands = seedRollingZeroOnlyAt(7);
    CHECK(grows(9, lands));
    // parity: `SaplingBlock.randomTick` asks for light nine above it, not at it.
    CHECK_FALSE(grows(8, lands));

    // A roll that misses leaves it alone, and the roll is one in seven.
    std::int64_t misses = 1;
    while (rng::RandomSource::legacy(misses).nextInt(7) == 0) {
        misses++;
    }
    CHECK_FALSE(grows(9, misses));
}

TEST_CASE("a cactus at its last age puts a fresh one over itself and starts again") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 63, 8, stateOf("sand")));
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("cactus", {{"age", "15"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(changes.size() == 2);
    CHECK(changes[0].y == 65);
    CHECK(pathOf(changes[0].stateId) == "cactus");
    CHECK(propertyOf(changes[0].stateId, "age") == "0");
    CHECK(changes[1].y == 64);
    CHECK(propertyOf(changes[1].stateId, "age") == "0");
}

TEST_CASE("a cactus with anything over it does nothing at all") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 63, 8, stateOf("sand")));
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("cactus", {{"age", "8"}})));
    REQUIRE(loaded.world.setBlockState(8, 65, 8, stateOf("stone")));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingBetween(0.0, 0.1));

    CHECK(ticks.tickBlock(context, 8, 64, 8, random).empty());
}

TEST_CASE("a cactus below the flowering age just ages") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    REQUIRE(loaded.world.setBlockState(8, 63, 8, stateOf("sand")));
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("cactus", {{"age", "7"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(seedRollingBetween(0.0, 0.1));

    const std::vector<BlockChange> changes = ticks.tickBlock(context, 8, 64, 8, random);

    REQUIRE(changes.size() == 1);
    CHECK(changes[0].y == 64);
    CHECK(propertyOf(changes[0].stateId, "age") == "8");
}

TEST_CASE("water reaches farmland four blocks out on either axis") {
    const auto wets = [](std::int32_t dx, std::int32_t dz) {
        Loaded loaded;
        loaded.world.getOrGenerateChunk(0, 0);
        loaded.world.getOrGenerateChunk(-1, -1);
        REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("farmland", {{"moisture", "1"}})));
        REQUIRE(loaded.world.setBlockState(8 + dx, 64, 8 + dz, registry::blocks::water().stateId));
        randomTicks::Context context = loaded.context();
        RandomTicks ticks;
        rng::RandomSource random = rng::RandomSource::legacy(1);
        return propertyOf(ticks.tickBlock(context, 8, 64, 8, random)[0].stateId, "moisture") == "7";
    };

    CHECK(wets(4, 0));
    CHECK(wets(-4, 0));
    CHECK(wets(0, 4));
    CHECK(wets(0, -4));
    CHECK(wets(4, 4));
    CHECK_FALSE(wets(5, 0));
    CHECK_FALSE(wets(0, 5));
}

TEST_CASE("every family that lives in its own file is reached by its own blocks") {
    // A family that is reached takes its turn at the random; one that is not leaves it alone. That
    // is enough to say the dispatch found it without asking each family what it did.
    const auto draws = [](const std::string& path, const std::map<std::string, std::string>& properties, bool inWater) {
        Loaded loaded;
        loaded.world.getOrGenerateChunk(0, 0);
        if (inWater) {
            for (std::int32_t y = 60; y < 70; y++) {
                REQUIRE(loaded.world.setBlockState(8, y, 8, registry::blocks::water().stateId));
            }
        }
        REQUIRE(loaded.world.setBlockState(8, 64, 8, properties.empty() ? stateOf(path) : stateOf(path, properties)));
        randomTicks::Context context = loaded.context();
        RandomTicks ticks;
        rng::RandomSource random = rng::RandomSource::legacy(9);
        (void)ticks.tickBlock(context, 8, 64, 8, random);
        rng::RandomSource fresh = rng::RandomSource::legacy(9);
        return random.nextInt(1000) != fresh.nextInt(1000);
    };

    CHECK(draws("red_mushroom", {}, false));
    CHECK(draws("brown_mushroom", {}, false));
    CHECK(draws("melon_stem", {{"age", "3"}}, false));
    CHECK(draws("pumpkin_stem", {{"age", "3"}}, false));
    CHECK(
        draws("sulfur_spike", {{"thickness", "tip"}, {"vertical_direction", "down"}, {"waterlogged", "false"}}, false));
    CHECK(draws("pointed_dripstone", {{"thickness", "tip"}, {"vertical_direction", "down"}, {"waterlogged", "false"}},
                false));
    CHECK(draws("cave_vines", {{"age", "0"}, {"berries", "false"}}, false));
    CHECK(draws("weeping_vines", {{"age", "0"}}, false));
    CHECK(draws("twisting_vines", {{"age", "0"}}, false));
    CHECK(draws("kelp", {{"age", "0"}}, true));
}

TEST_CASE("kelp climbs and weeping vines fall, which is what tells the two apart") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    for (std::int32_t y = 60; y < 70; y++) {
        REQUIRE(loaded.world.setBlockState(8, y, 8, registry::blocks::water().stateId));
    }
    REQUIRE(loaded.world.setBlockState(8, 64, 8, stateOf("kelp", {{"age", "0"}})));
    REQUIRE(loaded.world.setBlockState(2, 64, 2, stateOf("weeping_vines", {{"age", "0"}})));
    randomTicks::Context context = loaded.context();
    RandomTicks ticks;

    bool climbed = false;
    bool fell = false;
    // parity aside: one random for the whole run. A fresh one per turn rolls almost the same value
    // every time, because `java.util.Random` barely moves its first draw between neighbouring seeds.
    rng::RandomSource random = rng::RandomSource::legacy(1);
    for (std::int32_t turn = 0; turn < 400 && !(climbed && fell); turn++) {
        for (const BlockChange& change : ticks.tickBlock(context, 8, 64, 8, random)) {
            if (change.y == 65) {
                climbed = true;
            }
        }
        for (const BlockChange& change : ticks.tickBlock(context, 2, 64, 2, random)) {
            if (change.y == 63) {
                fell = true;
            }
        }
    }

    CHECK(climbed);
    CHECK(fell);
}

TEST_CASE("the bottom section of a chunk is swept like any other") {
    Loaded loaded;
    loaded.world.getOrGenerateChunk(0, 0);
    // parity: `tickChunk` walks every section the chunk has, and the first one is at the world's
    // floor. A sweep that started at the second would leave bedrock-level farmland forever wet.
    for (std::int32_t x = 0; x < 16; x++) {
        for (std::int32_t z = 0; z < 16; z++) {
            for (std::int32_t y = -64; y < -48; y++) {
                REQUIRE(loaded.world.setBlockState(x, y, z, stateOf("farmland", {{"moisture", "3"}})));
            }
        }
    }
    randomTicks::Context context = loaded.context();
    context.seed = 31337;
    const std::array<std::array<std::int32_t, 2>, 1> chunks{{{0, 0}}};
    RandomTicks ticks;
    rng::RandomSource random = rng::RandomSource::legacy(2);

    const std::vector<BlockChange> changes = ticks.tick(context, chunks, 2, random);

    REQUIRE(changes.size() == 2);
    for (const BlockChange& change : changes) {
        CHECK(change.y >= -64);
        CHECK(change.y < -48);
    }
}
