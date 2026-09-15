#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Dripleaves.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t leafOf(const std::string& tilt) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("big_dripleaf"), {{"tilt", tilt}, {"facing", "east"}, {"waterlogged", "false"}},
        std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string tiltAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("tilt");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// Runs the world's clock forward through whatever the leaf booked for itself.
void settle(WorldState& world, std::int32_t turns) {
    for (std::int32_t turn = 0; turn < turns; ++turn) {
        // parity: the turn opens before anything runs, and that is where the countdown happens.
        world.openTurn();
        for (const std::array<std::int32_t, 3>& at : world.dueBlockTicks()) {
            static_cast<void>(dripleaves::tick(world, "world", at[0], at[1], at[2], 1));
        }
    }
}

} // namespace

TEST_CASE("a leaf tipped over comes back up on its own, a hundred ticks later") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, leafOf("none")));

    // parity: `setTiltAndScheduleTick` books `DELAY_UNTIL_NEXT_TILT_STATE`, a hundred ticks lying
    // flat. Without that booking a leaf tipped by an arrow stayed down for ever.
    REQUIRE(dripleaves::setTilt(world, 8, 64, 8, "full", 1).has_value());
    CHECK(tiltAt(world, 8, 64, 8) == "full");

    settle(world, 99);
    CHECK(tiltAt(world, 8, 64, 8) == "full");
    settle(world, 1);
    CHECK(tiltAt(world, 8, 64, 8) == "none");
    // Upright it books nothing, so nothing else happens.
    settle(world, 200);
    CHECK(tiltAt(world, 8, 64, 8) == "none");
}

TEST_CASE("the way down is unstable, partial, full, ten ticks each") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, leafOf("none")));

    REQUIRE(dripleaves::setTilt(world, 8, 64, 8, "unstable", 1).has_value());
    settle(world, 10);
    CHECK(tiltAt(world, 8, 64, 8) == "partial");
    settle(world, 10);
    CHECK(tiltAt(world, 8, 64, 8) == "full");
}

TEST_CASE("a signal holds the leaf up, and lets it go when it stops") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, leafOf("full")));

    // parity: `BigDripleafBlock.neighborChanged`, the whole of it: with a signal the leaf goes back
    // up. This is what makes a dripleaf a redstone block, and none of it was modelled.
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::named("redstone_block")->stateId));
    const std::optional<BlockChange> flat = dripleaves::react(world, "world", 8, 64, 8, 1);
    REQUIRE(flat.has_value());
    CHECK(tiltAt(world, 8, 64, 8) == "none");

    // parity: `tick` asks the signal again, so a held leaf that is tipped goes straight back up
    // instead of walking the stages.
    REQUIRE(dripleaves::setTilt(world, 8, 64, 8, "unstable", 1).has_value());
    settle(world, 10);
    CHECK(tiltAt(world, 8, 64, 8) == "none");

    // With nothing powering it, the stages run as they should.
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    REQUIRE(dripleaves::setTilt(world, 8, 64, 8, "unstable", 1).has_value());
    settle(world, 10);
    CHECK(tiltAt(world, 8, 64, 8) == "partial");
}

TEST_CASE("no signal is no reaction at all") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, leafOf("full")));

    CHECK_FALSE(dripleaves::react(world, "world", 8, 64, 8, 1).has_value());
    CHECK(tiltAt(world, 8, 64, 8) == "full");
}

TEST_CASE("something standing on an upright leaf starts it tipping, unless it is held up") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, leafOf("none")));

    // parity: `canEntityTilt` wants both feet on the ground and a height past eleven sixteenths, so
    // brushing the leaf from below is not standing on it.
    CHECK_FALSE(dripleaves::stoodOn(world, "world", 8, 64, 8, 64.5, true, 1).has_value());
    CHECK_FALSE(dripleaves::stoodOn(world, "world", 8, 64, 8, 64.9375, false, 1).has_value());

    REQUIRE(dripleaves::stoodOn(world, "world", 8, 64, 8, 64.9375, true, 1).has_value());
    CHECK(tiltAt(world, 8, 64, 8) == "unstable");

    // Already tipping, so standing on it again says nothing.
    CHECK_FALSE(dripleaves::stoodOn(world, "world", 8, 64, 8, 64.9375, true, 1).has_value());

    // parity: a leaf with a signal on it does not start tipping at all. That is what makes a
    // dripleaf a gate and not a trapdoor that always drops.
    REQUIRE(world.setBlockState(8, 64, 8, leafOf("none")));
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::named("redstone_block")->stateId));
    CHECK_FALSE(dripleaves::stoodOn(world, "world", 8, 64, 8, 64.9375, true, 1).has_value());
    CHECK(tiltAt(world, 8, 64, 8) == "none");
}
