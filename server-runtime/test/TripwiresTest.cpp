#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Tripwires.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <doctest.h>

#include <array>
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
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
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
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
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
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
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
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "false");
    CHECK(propertyAt(world, 6, 64, 8, "attached") == "false");
}

TEST_CASE("standing on the string powers both hooks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);
    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
    changes.clear();

    tripwires::press(world, 6, 64, 8, true, 1, changes);
    CHECK_FALSE(changes.empty());
    CHECK(propertyAt(world, 6, 64, 8, "powered") == "true");
    // parity: the hook is the part that gives off redstone, and both ends of the line follow.
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "true");
    // A pressed wire books its own look again.
    CHECK(world.hasScheduledBlockTick(6, 64, 8, Identifier::vanilla("tripwire")));

    // Still stood on: nothing changed, so nothing is reported and the whole line is left alone.
    changes.clear();
    tripwires::press(world, 6, 64, 8, true, 1, changes);
    CHECK(changes.empty());

    // And letting go puts both hooks out.
    changes.clear();
    tripwires::press(world, 6, 64, 8, false, 1, changes);
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
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
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
    tripwires::press(world, 6, 64, 8, true, 1, changes);
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
    tripwires::press(world, 4, 64, 8, true, 1, changes);
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
    tripwires::press(world, 500, 64, 500, true, 1, changes);
    tripwires::calculateHook(world, 500, 64, 500, 1, changes);
    CHECK(changes.empty());
}

TEST_CASE("a hook that clicks tells a sculk sensor, and the click wins over the catch") {
    // parity: `TripWireHookBlock.emitState` is an else-if chain over four events, so a hook that both
    // catches and clicks on the same call raises **one**: `BLOCK_ACTIVATE`. Both ends of the line
    // raise it, and both read the near hook's own old state. None of them were raised here.
    const auto sleepingSensor = [] {
        const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
            Identifier::vanilla("sculk_sensor"),
            {{"sculk_sensor_phase", "inactive"}, {"power", "0"}, {"waterlogged", "false"}}, std::nullopt);
        REQUIRE(state.has_value());
        return *state;
    };
    const auto frequencyAt = [](const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
        const Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        const basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
        return entity == nullptr ? -1 : entity->data().getInt(std::string{vibrations::kFrequencyKey}, -1);
    };
    const auto settle = [](WorldState& world, std::int64_t& now, std::int32_t turns) {
        for (std::int32_t turn = 0; turn < turns; ++turn) {
            ++now;
            for (const std::array<std::int32_t, 3>& at : world.dueBlockTicks()) {
                static_cast<void>(vibrations::tick(world, at[0], at[1], at[2], now));
            }
        }
    };

    SUBCASE("something stepping on the string is a block_activate") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        layLine(world, 3);
        REQUIRE(world.setBlockState(4, 64, 10, sleepingSensor()));
        std::vector<BlockChange> changes;
        std::int64_t now = 1;
        tripwires::calculateHook(world, 4, 64, 8, now, changes);

        tripwires::press(world, 5, 64, 8, true, now, changes);
        REQUIRE(propertyAt(world, 4, 64, 8, "powered") == "true");
        settle(world, now, 4);

        CHECK(propertyAt(world, 4, 64, 10, "sculk_sensor_phase") == "active");
        CHECK(frequencyAt(world, 4, 64, 10) == 10);
    }

    SUBCASE("stepping off it is a block_deactivate") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        layLine(world, 3);
        std::vector<BlockChange> changes;
        std::int64_t now = 1;
        tripwires::calculateHook(world, 4, 64, 8, now, changes);
        tripwires::press(world, 5, 64, 8, true, now, changes);
        REQUIRE(propertyAt(world, 4, 64, 8, "powered") == "true");
        // The sensor goes down after the line is already pressed, so what it hears is the release.
        REQUIRE(world.setBlockState(4, 64, 10, sleepingSensor()));

        tripwires::press(world, 5, 64, 8, false, now, changes);
        REQUIRE(propertyAt(world, 4, 64, 8, "powered") == "false");
        settle(world, now, 4);

        CHECK(frequencyAt(world, 4, 64, 10) == 9);
    }
}

TEST_CASE("a wire tells the hooks at the ends of its line, and only to the south and west") {
    // parity: `TripWireBlock.updateSource`. A tripwire laid by hand or by command has to attach its
    // hooks by itself; nothing here did it, so a line built and never stepped on hung loose.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "false");

    std::vector<BlockChange> changes;
    const std::int32_t wire = *world.blockState(5, 64, 8);
    tripwires::updateSource(world, 5, 64, 8, wire, 1, changes);

    // The hook it reaches going west is the one at x=4, and writing it writes the pair.
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "attached") == "true");
}

TEST_CASE("a hook told about a wire holds on for ten ticks and then lets go") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);
    std::vector<BlockChange> changes;
    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
    REQUIRE(propertyAt(world, 4, 64, 8, "attached") == "true");

    // The middle string goes, and the hook is told with the state it had, powered, as vanilla does
    // from `affectNeighborsAfterRemoval`. parity: `calculateState` reads that square from what it
    // was handed, so on this pass the line is whole and **fires**.
    const std::int32_t was = *world.blockState(6, 64, 8);
    const std::optional<std::int32_t> firing =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla("tripwire"), {{"powered", "true"}}, was);
    REQUIRE(firing.has_value());
    REQUIRE(world.setBlockState(6, 64, 8, registry::blocks::air().stateId));
    tripwires::updateSource(world, 6, 64, 8, *firing, 1, changes);

    CHECK(propertyAt(world, 4, 64, 8, "attached") == "true");
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "true");
    // And it booked itself a look ten ticks out, which is what finds the gap.
    CHECK(world.hasScheduledBlockTick(4, 64, 8, Identifier::vanilla("tripwire_hook")));
    for (std::int32_t turn = 0; turn < 9; ++turn) {
        CHECK(world.dueBlockTicks().empty());
    }
    REQUIRE(world.dueBlockTicks().size() == 1);

    tripwires::calculateHook(world, 4, 64, 8, 1, changes);
    CHECK(propertyAt(world, 4, 64, 8, "attached") == "false");
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "false");
}

TEST_CASE("a string draws its own line, and a hook only falls from the side it hangs on") {
    // parity: `TripWireBlock.updateShape` and `TripWireHookBlock.updateShape`. Measured in
    // `ganchosensor`: the jar's string had `east` and `west` true and ours had neither, and the jar's
    // hook survived being put down in mid air while ours broke and fell as an item.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    layLine(world, 3);

    /// A string joined to nothing, which is how vanilla's own default is drawn.
    const auto looseWire = [] {
        const std::optional<std::int32_t> state =
            registry::blockStateDefinitions::stateIdOf(Identifier::vanilla("tripwire"),
                                                       {{"powered", "false"},
                                                        {"attached", "false"},
                                                        {"disarmed", "false"},
                                                        {"north", "false"},
                                                        {"south", "false"},
                                                        {"east", "false"},
                                                        {"west", "false"}},
                                                       std::nullopt);
        REQUIRE(state.has_value());
        return *state;
    };

    SUBCASE("the string follows what is beside it") {
        REQUIRE(world.setBlockState(5, 64, 8, looseWire()));
        const std::optional<std::int32_t> drawn =
            tripwires::reshape(world, 5, 64, 8, *world.blockState(5, 64, 8), registry::BlockFace::West);
        REQUIRE(drawn.has_value());
        REQUIRE(world.setBlockState(5, 64, 8, *drawn));
        // West of it is the hook at x=4 facing east, which points back: that side connects.
        CHECK(propertyAt(world, 5, 64, 8, "west") == "true");
        // The side nobody named is left where it was.
        CHECK(propertyAt(world, 5, 64, 8, "east") == "false");

        // The whole line at once when no side is named.
        REQUIRE(world.setBlockState(6, 64, 8, looseWire()));
        const std::optional<std::int32_t> whole =
            tripwires::reshape(world, 6, 64, 8, *world.blockState(6, 64, 8), std::nullopt);
        REQUIRE(whole.has_value());
        REQUIRE(world.setBlockState(6, 64, 8, *whole));
        CHECK(propertyAt(world, 6, 64, 8, "east") == "true");
        CHECK(propertyAt(world, 6, 64, 8, "west") == "true");
        CHECK(propertyAt(world, 6, 64, 8, "north") == "false");
    }

    SUBCASE("from above or below the string is left alone") {
        REQUIRE(world.setBlockState(5, 64, 8, looseWire()));
        CHECK_FALSE(
            tripwires::reshape(world, 5, 64, 8, *world.blockState(5, 64, 8), registry::BlockFace::Up).has_value());
    }

    SUBCASE("the hook hangs on the block behind it and falls only from there") {
        // The hook at x=4 faces east, so it hangs on x=3, which is air: it cannot survive.
        const std::int32_t hook = *world.blockState(4, 64, 8);
        CHECK(tripwires::hookFalls(world, 4, 64, 8, hook, registry::BlockFace::West));
        // News from any other side leaves it where it is.
        CHECK_FALSE(tripwires::hookFalls(world, 4, 64, 8, hook, registry::BlockFace::East));
        CHECK_FALSE(tripwires::hookFalls(world, 4, 64, 8, hook, registry::BlockFace::North));
        CHECK_FALSE(tripwires::hookFalls(world, 4, 64, 8, hook, registry::BlockFace::Up));

        // With a wall behind it, it survives news from that side too.
        REQUIRE(world.setBlockState(3, 64, 8, stateOf("stone")));
        CHECK_FALSE(tripwires::hookFalls(world, 4, 64, 8, hook, registry::BlockFace::West));
    }
}
