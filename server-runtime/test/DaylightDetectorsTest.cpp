#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t detectorOf(const std::string& inverted, const std::string& power = "0") {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("daylight_detector"), {{"inverted", inverted}, {"power", power}}, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string powerAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("power");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// Loads the nine chunks around the origin, so the light at the middle of one is not the light of
/// an open border.
void openSky(WorldState& world) {
    for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            world.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
}

[[nodiscard]] WorldClock at(std::int64_t ticks) { return WorldClock{ticks, 1.0F, false}; }

/// What the world would have stored for that clock and weather, which is what a reader gets.
[[nodiscard]] std::int32_t darken(const WorldClock& clock, const WorldWeather& weather) {
    return daylightDetectors::skyDarken(clock, weather);
}

} // namespace

TEST_CASE("only the overworld has a night") {
    // parity: the `minecraft:day` timeline is tagged `#minecraft:in_overworld` and nothing else, so
    // the Nether sits at its own four and the End at the attribute's default fifteen, both unmoving.
    const WorldWeather clear;
    const daylightDetectors::Sky nether{.level = 4.0F, .dayTimeline = false};
    const daylightDetectors::Sky end{.level = 15.0F, .dayTimeline = false};
    for (const std::int64_t ticks : {0, 6000, 13000, 18000, 23000}) {
        CHECK(daylightDetectors::skyDarken(at(ticks), clear, nether) == 11);
        CHECK(daylightDetectors::skyDarken(at(ticks), clear, end) == 0);
    }

    // A detector in the End reads the full fifteen at every hour: the sun angle is the attribute's
    // own zero there, so nothing bends the reading either.
    WorldState world = emptyWorld();
    openSky(world);
    REQUIRE(world.setBlockState(8, 64, 8, detectorOf("false")));
    for (const std::int64_t ticks : {0, 6000, 18000}) {
        CHECK(daylightDetectors::signalFor(world, at(ticks), 0, 8, 64, 8, false, true, false) == 15);
    }
}

TEST_CASE("a detector reads most at noon and nothing at night") {
    WorldState world = emptyWorld();
    openSky(world);
    const WorldWeather clear;
    REQUIRE(world.setBlockState(8, 64, 8, detectorOf("false")));

    // parity: the reading is bent by the angle of the sun, so noon is the peak and not a plateau.
    const std::int32_t noon = daylightDetectors::signalFor(world, at(6000), darken(at(6000), clear), 8, 64, 8, false);
    CHECK(noon == 15);
    const std::int32_t morning =
        daylightDetectors::signalFor(world, at(1000), darken(at(1000), clear), 8, 64, 8, false);
    CHECK(morning > 0);
    CHECK(morning < noon);
    const std::int32_t midnight =
        daylightDetectors::signalFor(world, at(18000), darken(at(18000), clear), 8, 64, 8, false);
    CHECK(midnight == 0);
}

TEST_CASE("the reading follows the sun through the day") {
    WorldState world = emptyWorld();
    openSky(world);
    const WorldWeather clear;
    // parity: `Level.getSunAngle` over `DimensionType.timeOfDay`. The curve is not a straight line
    // and not a plateau: it climbs to fifteen at noon, and the same reading comes round twice a day
    // on the way up and the way down.
    const std::vector<std::pair<std::int64_t, std::int32_t>> readings{{0, 7},     {1000, 10},  {3000, 13}, {6000, 15},
                                                                      {9000, 13}, {11000, 10}, {12000, 7}, {13000, 2},
                                                                      {18000, 0}, {23000, 2}};
    for (const auto& [ticks, expected] : readings) {
        CAPTURE(ticks);
        CHECK(daylightDetectors::signalFor(world, at(ticks), darken(at(ticks), clear), 8, 64, 8, false) == expected);
    }
}

TEST_CASE("a detector reads the sky above itself, not its own square") {
    WorldState world = emptyWorld();
    openSky(world);
    const WorldWeather clear;
    // One block right on top of it. Its own square still catches plenty of light from the sides,
    // and the square it actually reads is inside the stone: a covered detector reads nothing.
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));
    REQUIRE(worldLight::skyLight(world, 8, 64, 8) > worldLight::skyLight(world, 8, 65, 8));
    CHECK(daylightDetectors::signalFor(world, at(6000), darken(at(6000), clear), 8, 64, 8, false) == 0);
}

TEST_CASE("an inverted detector is the other way round and skips the curve") {
    WorldState world = emptyWorld();
    openSky(world);
    const WorldWeather clear;

    // parity: fifteen minus the sky brightness, with no sun angle at all, which is what makes it a
    // clean night switch. Night takes eleven off the sky rather than all fifteen, so an inverted
    // detector reads eleven and not the full scale.
    CHECK(daylightDetectors::signalFor(world, at(6000), darken(at(6000), clear), 8, 64, 8, true) == 0);
    CHECK(daylightDetectors::signalFor(world, at(18000), darken(at(18000), clear), 8, 64, 8, true) == 11);
}

TEST_CASE("rain and thunder each take their share of the daylight") {
    const WorldClock noon = at(6000);
    const WorldClock midnight = at(18000);
    WorldWeather clear;
    WorldWeather raining;
    raining.raining = true;
    raining.setRainLevel(1.0F);
    WorldWeather storm;
    storm.raining = true;
    storm.thundering = true;
    storm.setRainLevel(1.0F);
    storm.setThunderLevel(1.0F);

    // parity: `WeatherAttributes` blends `SKY_LIGHT_LEVEL` towards four with `ALPHA_BLEND`, so at
    // noon fifteen becomes 11.5625 under rain and 9.19921875 under a storm. The storm takes its own
    // level off the rain's, which is why the two do not stack.
    CHECK(daylightDetectors::skyDarken(noon, clear) == 0);
    CHECK(daylightDetectors::skyDarken(noon, raining) == 3);
    CHECK(daylightDetectors::skyDarken(noon, storm) == 5);

    // parity: at midnight the value is already four (fifteen by 0.26666668 rounds to it exactly in
    // a float), so blending towards four changes nothing and the rain stops counting. The old
    // product kept darkening a number that was already on the floor.
    CHECK(daylightDetectors::skyDarken(midnight, clear) == 11);
    CHECK(daylightDetectors::skyDarken(midnight, raining) == 11);

    WorldState world = emptyWorld();
    openSky(world);
    CHECK(daylightDetectors::signalFor(world, noon, darken(noon, storm), 8, 64, 8, false) == 10);
    CHECK(daylightDetectors::signalFor(world, noon, darken(noon, clear), 8, 64, 8, false) == 15);
}

TEST_CASE("a detector under a roof reads nothing at all") {
    WorldState world = emptyWorld();
    openSky(world);
    const WorldWeather clear;
    REQUIRE(world.setBlockState(8, 64, 8, detectorOf("false")));
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 70, z, stateOf("stone")));
        }
    }
    CHECK(daylightDetectors::signalFor(world, at(6000), darken(at(6000), clear), 8, 64, 8, false) < 15);
}

TEST_CASE("the block follows its reading, and says nothing when it has not moved") {
    WorldState world = emptyWorld();
    openSky(world);
    const WorldWeather clear;
    REQUIRE(world.setBlockState(8, 64, 8, detectorOf("false")));

    const std::optional<BlockChange> lit =
        daylightDetectors::update(world, at(6000), darken(at(6000), clear), 8, 64, 8);
    REQUIRE(lit.has_value());
    CHECK(powerAt(world, 8, 64, 8) == "15");
    CHECK_FALSE(daylightDetectors::update(world, at(6000), darken(at(6000), clear), 8, 64, 8).has_value());

    // Night falls and it goes out.
    const std::optional<BlockChange> dark =
        daylightDetectors::update(world, at(18000), darken(at(18000), clear), 8, 64, 8);
    REQUIRE(dark.has_value());
    CHECK(powerAt(world, 8, 64, 8) == "0");

    // What is not a detector is left alone, including a block that has a `power` of its own: a
    // weighted plate is not driven by the sky.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    CHECK_FALSE(daylightDetectors::update(world, at(6000), darken(at(6000), clear), 9, 64, 8).has_value());
    CHECK_FALSE(daylightDetectors::update(world, at(6000), darken(at(6000), clear), 500, 64, 500).has_value());
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("light_weighted_pressure_plate")));
    CHECK_FALSE(daylightDetectors::update(world, at(6000), darken(at(6000), clear), 10, 64, 8).has_value());
    CHECK(powerAt(world, 10, 64, 8) == "0");
}

TEST_CASE("a world with no sky never reads anything") {
    WorldState world = emptyWorld();
    openSky(world);
    const WorldWeather clear;
    REQUIRE(world.setBlockState(8, 64, 8, detectorOf("false")));
    CHECK(daylightDetectors::signalFor(world, at(6000), darken(at(6000), clear), 8, 64, 8, false, false) == 0);
    // Inverted, no sky is the full fifteen: down there it is always night.
    CHECK(daylightDetectors::signalFor(world, at(6000), darken(at(6000), clear), 8, 64, 8, true, false) == 15);
}
