#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ClimbingPlantGrowth.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, stateOf(path));
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string propertyAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const std::string& name) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string pathAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const Identifier* id = registry::blockStateDefinitions::blockOf(*stateId);
    return id == nullptr ? std::string{} : id->path();
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }

    void put(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        REQUIRE(state.setBlockState(x, y, z, stateId));
    }
};

/// A random that always rolls low enough to grow, and one that never does.
[[nodiscard]] rng::RandomSource always() { return rng::RandomSource::legacy(1); }

} // namespace

TEST_CASE("a vine that climbs leaves its own body behind it") {
    Loaded loaded;
    const std::int32_t head = stateOf("weeping_vines", {{"age", "3"}});
    loaded.put(8, 64, 8, head);
    rng::RandomSource random = always();
    std::vector<BlockChange> changes;

    climbingPlantGrowth::growingPlant(loaded.state, 8, 64, 8, head, false, 1.0, random, changes);

    // The head moves down and the block it left is the plant's own body.
    CHECK(pathAt(loaded.state, 8, 63, 8) == "weeping_vines");
    CHECK(propertyAt(loaded.state, 8, 63, 8, "age") == "4");
    CHECK(pathAt(loaded.state, 8, 64, 8) == "weeping_vines_plant");
    CHECK(changes.size() == 2);
}

TEST_CASE("twisting vines climb the other way") {
    Loaded loaded;
    const std::int32_t head = stateOf("twisting_vines", {{"age", "0"}});
    loaded.put(8, 64, 8, head);
    rng::RandomSource random = always();
    std::vector<BlockChange> changes;

    climbingPlantGrowth::growingPlant(loaded.state, 8, 64, 8, head, true, 1.0, random, changes);

    CHECK(pathAt(loaded.state, 8, 65, 8) == "twisting_vines");
    CHECK(pathAt(loaded.state, 8, 64, 8) == "twisting_vines_plant");
}

TEST_CASE("kelp grows into water and nothing else, and the rest grow into air and nothing else") {
    Loaded water;
    const std::int32_t kelp = stateOf("kelp", {{"age", "0"}});
    water.put(8, 64, 8, kelp);
    water.put(8, 65, 8, stateOf("water", {{"level", "0"}}));
    rng::RandomSource wet = always();
    std::vector<BlockChange> grew;
    climbingPlantGrowth::growingPlant(water.state, 8, 64, 8, kelp, true, 1.0, wet, grew);
    CHECK(pathAt(water.state, 8, 65, 8) == "kelp");

    // parity: kelp will not climb into air, which is the one thing everything else needs.
    Loaded air;
    air.put(8, 64, 8, kelp);
    rng::RandomSource dry = always();
    std::vector<BlockChange> none;
    climbingPlantGrowth::growingPlant(air.state, 8, 64, 8, kelp, true, 1.0, dry, none);
    CHECK(none.empty());

    // And a vine will not climb into water.
    Loaded flooded;
    const std::int32_t vine = stateOf("twisting_vines", {{"age", "0"}});
    flooded.put(8, 64, 8, vine);
    flooded.put(8, 65, 8, stateOf("water", {{"level", "0"}}));
    rng::RandomSource drowned = always();
    std::vector<BlockChange> blocked;
    climbingPlantGrowth::growingPlant(flooded.state, 8, 64, 8, vine, true, 1.0, drowned, blocked);
    CHECK(blocked.empty());
}

TEST_CASE("a vine that has climbed as far as it goes stops there") {
    Loaded loaded;
    const std::int32_t grown = stateOf("twisting_vines", {{"age", "25"}});
    loaded.put(8, 64, 8, grown);
    rng::RandomSource random = always();
    std::vector<BlockChange> changes;

    climbingPlantGrowth::growingPlant(loaded.state, 8, 64, 8, grown, true, 1.0, random, changes);

    // parity: twenty-five is the last age, and it does not draw a roll to find that out.
    CHECK(changes.empty());
    CHECK(random.nextDouble() == doctest::Approx(always().nextDouble()));
}

TEST_CASE("a roll that misses leaves the vine where it is") {
    Loaded loaded;
    const std::int32_t head = stateOf("twisting_vines", {{"age", "0"}});
    loaded.put(8, 64, 8, head);
    rng::RandomSource random = always();
    std::vector<BlockChange> changes;

    // A chance of nothing is a chance nothing ever beats.
    climbingPlantGrowth::growingPlant(loaded.state, 8, 64, 8, head, true, 0.0, random, changes);

    CHECK(changes.empty());
    CHECK(pathAt(loaded.state, 8, 64, 8) == "twisting_vines");
}

TEST_CASE("something that is not a climbing plant climbs nowhere") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    rng::RandomSource random = always();
    std::vector<BlockChange> changes;

    climbingPlantGrowth::growingPlant(loaded.state, 8, 64, 8, stateOf("stone"), true, 1.0, random, changes);

    CHECK(changes.empty());
}

TEST_CASE("copper weathers one step at a time") {
    Loaded loaded;
    const std::int32_t fresh = stateOf("copper_block");
    loaded.put(8, 64, 8, fresh);
    // A random whose first two draws are both low enough.
    rng::RandomSource random = rng::RandomSource::legacy(3);
    std::vector<BlockChange> changes;

    for (std::int32_t attempt = 0; attempt < 200 && changes.empty(); ++attempt) {
        weatheringGrowth::weather(loaded.state, 8, 64, 8, fresh, random, changes);
    }

    REQUIRE_FALSE(changes.empty());
    CHECK(pathAt(loaded.state, 8, 64, 8) == "exposed_copper");
}

TEST_CASE("a block that does not weather is left alone, however often it is asked") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    rng::RandomSource random = rng::RandomSource::legacy(3);
    std::vector<BlockChange> changes;

    for (std::int32_t attempt = 0; attempt < 200; ++attempt) {
        weatheringGrowth::weather(loaded.state, 8, 64, 8, stateOf("stone"), random, changes);
    }

    CHECK(changes.empty());
}

TEST_CASE("one younger neighbour within four blocks holds the whole thing back") {
    const auto weathersWith = [](std::int32_t dx, std::int32_t dy, std::int32_t dz, const std::string& neighbour) {
        Loaded loaded;
        const std::int32_t exposed = stateOf("exposed_copper");
        loaded.put(8, 64, 8, exposed);
        loaded.put(8 + dx, 64 + dy, 8 + dz, stateOf(neighbour));
        rng::RandomSource random = rng::RandomSource::legacy(3);
        std::vector<BlockChange> changes;
        for (std::int32_t attempt = 0; attempt < 500 && changes.empty(); ++attempt) {
            weatheringGrowth::weather(loaded.state, 8, 64, 8, exposed, random, changes);
        }
        return !changes.empty();
    };

    // parity: a fresher block within the four-block diamond stops it outright.
    CHECK_FALSE(weathersWith(4, 0, 0, "copper_block"));
    CHECK_FALSE(weathersWith(1, 2, 1, "copper_block"));
    // Outside the diamond it counts for nothing.
    CHECK(weathersWith(5, 0, 0, "copper_block"));
    CHECK(weathersWith(2, 2, 2, "copper_block"));
    // An older one does not hold it back; it hurries it along.
    CHECK(weathersWith(4, 0, 0, "oxidized_copper"));
}

TEST_CASE("weathering keeps whatever else the block was carrying") {
    Loaded loaded;
    const std::int32_t stairs = stateOf("copper_grate");
    loaded.put(8, 64, 8, stairs);
    rng::RandomSource random = rng::RandomSource::legacy(3);
    std::vector<BlockChange> changes;

    for (std::int32_t attempt = 0; attempt < 500 && changes.empty(); ++attempt) {
        weatheringGrowth::weather(loaded.state, 8, 64, 8, stairs, random, changes);
    }

    REQUIRE_FALSE(changes.empty());
    CHECK(pathAt(loaded.state, 8, 64, 8) == "exposed_copper_grate");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "waterlogged") == "false");
}

TEST_CASE("what is already as weathered as it gets stays there") {
    Loaded loaded;
    const std::int32_t oxidized = stateOf("oxidized_copper");
    loaded.put(8, 64, 8, oxidized);
    rng::RandomSource random = rng::RandomSource::legacy(3);
    std::vector<BlockChange> changes;

    for (std::int32_t attempt = 0; attempt < 500; ++attempt) {
        weatheringGrowth::weather(loaded.state, 8, 64, 8, oxidized, random, changes);
    }

    CHECK(changes.empty());
}

namespace {

/// How many turns a copper block at (8, 64, 8) takes to weather, with the neighbours put down first.
[[nodiscard]] std::int32_t turnsToWeather(const std::vector<std::array<std::int32_t, 3>>& neighbours,
                                          const std::string& neighbour = "copper_block") {
    Loaded loaded;
    const std::int32_t fresh = stateOf("copper_block");
    loaded.put(8, 64, 8, fresh);
    for (const std::array<std::int32_t, 3>& at : neighbours) {
        loaded.put(8 + at[0], 64 + at[1], 8 + at[2], stateOf(neighbour));
    }
    rng::RandomSource random = rng::RandomSource::legacy(7);
    std::vector<BlockChange> changes;
    for (std::int32_t turn = 1; turn <= 5000; ++turn) {
        weatheringGrowth::weather(loaded.state, 8, 64, 8, fresh, random, changes);
        if (!changes.empty()) {
            return turn;
        }
    }
    return -1;
}

} // namespace

/// The turn the same rolls would land on, from the formula rather than from the port.
[[nodiscard]] std::int32_t expectedTurns(std::int32_t older, std::int32_t same) {
    rng::RandomSource random = rng::RandomSource::legacy(7);
    const float share = static_cast<float>(older + 1) / static_cast<float>(older + same + 1);
    for (std::int32_t turn = 1; turn <= 5000; ++turn) {
        if (random.nextFloat() >= 0.05688889F) {
            continue;
        }
        if (random.nextFloat() >= share * share) {
            continue;
        }
        return turn;
    }
    return -1;
}

TEST_CASE("how readily a block weathers is decided by how weathered its neighbours are") {
    // parity: `(older + 1) / (older + same + 1)`, squared, on top of the daily roll.
    CHECK(turnsToWeather({}) == expectedTurns(0, 0));
    CHECK(turnsToWeather({{1, 0, 0}}) == expectedTurns(0, 1));
    CHECK(turnsToWeather({{1, 0, 0}, {-1, 0, 0}}) == expectedTurns(0, 2));
    CHECK(turnsToWeather({{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}}) == expectedTurns(0, 3));

    // An older neighbour counts on the other side of the fraction.
    CHECK(turnsToWeather({{1, 0, 0}}, "oxidized_copper") == expectedTurns(1, 0));
    CHECK(turnsToWeather({{1, 0, 0}, {-1, 0, 0}}, "oxidized_copper") == expectedTurns(2, 0));

    // With nobody to hold it back the second roll always passes, so it is the fastest.
    CHECK(turnsToWeather({}) < turnsToWeather({{1, 0, 0}}));
}

TEST_CASE("the block itself is not one of its own neighbours") {
    // Were it counted, a lone block would read as one of its own kind and weather more slowly.
    CHECK(turnsToWeather({}) < turnsToWeather({{1, 0, 0}}));
}

TEST_CASE("the neighbourhood reaches four blocks on each axis, and no further") {
    const std::int32_t alone = turnsToWeather({});

    // On the edge of the diamond, on each axis in turn.
    CHECK(turnsToWeather({{4, 0, 0}}) != alone);
    CHECK(turnsToWeather({{0, 4, 0}}) != alone);
    CHECK(turnsToWeather({{0, 0, 4}}) != alone);
    CHECK(turnsToWeather({{0, -4, 0}}) != alone);
    // One further out on any axis is outside it.
    CHECK(turnsToWeather({{5, 0, 0}}) == alone);
    CHECK(turnsToWeather({{0, 5, 0}}) == alone);
    CHECK(turnsToWeather({{0, 0, 5}}) == alone);
    // And so is a corner whose three steps add up past four.
    CHECK(turnsToWeather({{2, 2, 1}}) == alone);
    CHECK(turnsToWeather({{2, 1, 1}}) != alone);
}
