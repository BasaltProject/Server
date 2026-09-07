#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
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

[[nodiscard]] std::int32_t wireOf(const std::string& powered, const std::string& attached,
                                  const std::string& disarmed = "false") {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("tripwire"), {{"powered", powered}, {"attached", attached}, {"disarmed", disarmed}},
        std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::int32_t hookOf(const std::string& facing, const std::string& powered = "false",
                                  const std::string& attached = "false") {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("tripwire_hook"), {{"facing", facing}, {"powered", powered}, {"attached", attached}},
        std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string propertyAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const std::string& key) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A line of hooks facing each other along x, with string between them.
void layLine(WorldState& world, std::int32_t length) {
    REQUIRE(world.setBlockState(4, 64, 8, hookOf("east")));
    for (std::int32_t at = 1; at <= length; ++at) {
        REQUIRE(world.setBlockState(4 + at, 64, 8, wireOf("false", "false")));
    }
    REQUIRE(world.setBlockState(4 + length + 1, 64, 8, hookOf("west")));
}

} // namespace

TEST_CASE("two hooks with string between them draw taut") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);

    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, changes);
    CHECK_FALSE(changes.empty());

    // parity: attached means the line reached the hook pointing back, string unbroken.
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "attached") == "true");
    for (std::int32_t at = 5; at <= 7; ++at) {
        CAPTURE(at);
        CHECK(propertyAt(world, at, 64, 8, "attached") == "true");
    }
    // Nothing on the string yet, so nothing is powered.
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "false");
}

TEST_CASE("a broken line is not attached at all") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);
    // Something that is not string in the middle of the run.
    REQUIRE(world.setBlockState(6, 64, 8, stateOf("stone")));

    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, changes);
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "false");
    CHECK(propertyAt(world, 8, 64, 8, "attached") == "false");
}

TEST_CASE("two hooks side by side are not a line") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `attached &= receiverPos > 1`, so a hook right next to another one is not attached.
    REQUIRE(world.setBlockState(4, 64, 8, hookOf("east")));
    REQUIRE(world.setBlockState(5, 64, 8, hookOf("west")));

    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, changes);
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "false");
}

TEST_CASE("a hook that does not point back is not the other end") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `calculateState` looks for the hook facing *back* along the line. One facing the same
    // way is a hook at the far end of somebody else's line, not the other end of this one.
    REQUIRE(world.setBlockState(4, 64, 8, hookOf("east")));
    for (std::int32_t at = 5; at <= 7; ++at) {
        REQUIRE(world.setBlockState(at, 64, 8, wireOf("false", "false")));
    }
    REQUIRE(world.setBlockState(8, 64, 8, hookOf("east")));

    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, changes);
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "false");
    CHECK(propertyAt(world, 6, 64, 8, "attached") == "false");
}

TEST_CASE("standing on the string powers both hooks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);
    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, changes);
    changes.clear();

    tripwires::press(world, 6, 64, 8, true, changes);
    CHECK_FALSE(changes.empty());
    CHECK(propertyAt(world, 6, 64, 8, "powered") == "true");
    // parity: the hook is the part that gives off redstone, and both ends of the line follow.
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "true");
    // A pressed wire books its own look again.
    CHECK(world.hasScheduledBlockTick(6, 64, 8, Identifier::vanilla("tripwire")));

    // Still stood on: nothing changed, so nothing is reported and the whole line is left alone.
    changes.clear();
    tripwires::press(world, 6, 64, 8, true, changes);
    CHECK(changes.empty());

    // And letting go puts both hooks out.
    changes.clear();
    tripwires::press(world, 6, 64, 8, false, changes);
    CHECK(propertyAt(world, 6, 64, 8, "powered") == "false");
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "false");
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "false");
}

TEST_CASE("string cut with shears powers nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);
    // parity: a disarmed wire is one somebody cut with shears; it still hangs, and it never powers.
    REQUIRE(world.setBlockState(6, 64, 8, wireOf("true", "true", "true")));

    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, changes);
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "true");
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "false");
}

TEST_CASE("a string with nothing at both ends powers nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A hook at one end only: no line, so standing on the string changes nothing at the hook.
    REQUIRE(world.setBlockState(4, 64, 8, hookOf("east")));
    for (std::int32_t at = 5; at <= 7; ++at) {
        REQUIRE(world.setBlockState(at, 64, 8, wireOf("false", "false")));
    }

    std::vector<BlockChange> changes;
    tripwires::press(world, 6, 64, 8, true, changes);
    CHECK(propertyAt(world, 6, 64, 8, "powered") == "true");
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "false");
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "false");
}

TEST_CASE("the box a wire is crossed in") {
    // A wire lies on the floor, so the whole block counts and a little below it.
    CHECK(tripwires::isStandingOn(0, 64, 0, 0.5, 64.0, 0.5));
    CHECK(tripwires::isStandingOn(0, 64, 0, 0.0, 64.0, 1.0));
    CHECK(tripwires::isStandingOn(0, 64, 0, 0.5, 63.95, 0.5));
    CHECK(tripwires::isStandingOn(0, 64, 0, 0.5, 65.0, 0.5));
    CHECK_FALSE(tripwires::isStandingOn(0, 64, 0, 0.5, 63.9, 0.5));
    CHECK_FALSE(tripwires::isStandingOn(0, 64, 0, 1.1, 64.0, 0.5));
    CHECK_FALSE(tripwires::isStandingOn(0, 64, 0, 0.5, 64.0, -0.1));
}

TEST_CASE("what is not a wire and what is not a hook") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 8, stateOf("stone")));

    std::vector<BlockChange> changes;
    tripwires::press(world, 4, 64, 8, true, changes);
    tripwires::calculateHook(world, 4, 64, 8, changes);
    tripwires::press(world, 500, 64, 500, true, changes);
    tripwires::calculateHook(world, 500, 64, 500, changes);
    CHECK(changes.empty());
}
