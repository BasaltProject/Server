#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::entity::BlockHit;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::int32_t targetOf(std::int32_t power) {
    return stateOf("target", {{"power", std::to_string(power)}});
}

[[nodiscard]] std::string propertyAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const std::string& name) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] BlockHit hitAt(double x, double y, double z, registry::BlockFace face) {
    return BlockHit{.x = x, .y = y, .z = z, .blockX = 8, .blockY = 64, .blockZ = 8, .face = face};
}

/// How many turns it takes for this position to come due, or zero within `limit`.
[[nodiscard]] std::int32_t ticksUntilDue(WorldState& world, std::int32_t limit) {
    for (std::int32_t tick = 1; tick <= limit; ++tick) {
        // parity: the turn opens before anything runs, and that is where the countdown happens.
        world.openTurn();
        const std::vector<std::array<std::int32_t, 3>> due = world.dueBlockTicks();
        if (!due.empty()) {
            CHECK(due.size() == 1);
            CHECK(due.front() == std::array<std::int32_t, 3>{8, 64, 8});
            return tick;
        }
    }
    return 0;
}

} // namespace

TEST_CASE("the middle of a target face is fifteen and its rim is one") {
    CHECK(projectileHits::signalFor(hitAt(8.5, 65.0, 8.5, registry::BlockFace::Up)) == 15);
    CHECK(projectileHits::signalFor(hitAt(8.25, 65.0, 8.5, registry::BlockFace::Up)) == 8);
    CHECK(projectileHits::signalFor(hitAt(8.375, 65.0, 8.5, registry::BlockFace::Up)) == 12);
    CHECK(projectileHits::signalFor(hitAt(8.5, 65.0, 8.125, registry::BlockFace::Up)) == 4);
    CHECK(projectileHits::signalFor(hitAt(8.0, 65.0, 8.5, registry::BlockFace::Up)) == 1);
    CHECK(projectileHits::signalFor(hitAt(8.5, 65.0, 9.0, registry::BlockFace::Up)) == 1);
}

TEST_CASE("which two distances count depends on the axis of the face hit") {
    // The coordinate that runs along the face is the one that does not count.
    CHECK(projectileHits::signalFor(hitAt(8.25, 64.5, 8.5, registry::BlockFace::Up)) == 8);
    CHECK(projectileHits::signalFor(hitAt(8.25, 64.5, 8.5, registry::BlockFace::North)) == 8);
    CHECK(projectileHits::signalFor(hitAt(8.25, 64.5, 8.5, registry::BlockFace::East)) == 15);

    CHECK(projectileHits::signalFor(hitAt(8.5, 64.25, 8.5, registry::BlockFace::Up)) == 15);
    CHECK(projectileHits::signalFor(hitAt(8.5, 64.25, 8.5, registry::BlockFace::North)) == 8);
    CHECK(projectileHits::signalFor(hitAt(8.5, 64.25, 8.5, registry::BlockFace::East)) == 8);

    CHECK(projectileHits::signalFor(hitAt(8.5, 64.5, 8.25, registry::BlockFace::Up)) == 8);
    CHECK(projectileHits::signalFor(hitAt(8.5, 64.5, 8.25, registry::BlockFace::North)) == 15);
    CHECK(projectileHits::signalFor(hitAt(8.5, 64.5, 8.25, registry::BlockFace::East)) == 8);

    // The other face of each pair reads the same, since only the axis is asked for.
    CHECK(projectileHits::signalFor(hitAt(8.25, 64.5, 8.5, registry::BlockFace::Down)) == 8);
    CHECK(projectileHits::signalFor(hitAt(8.25, 64.5, 8.5, registry::BlockFace::South)) == 8);
    CHECK(projectileHits::signalFor(hitAt(8.25, 64.5, 8.5, registry::BlockFace::West)) == 15);
}

TEST_CASE("an arrow in the middle of a target turns it up to fifteen for twenty ticks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, targetOf(0)));

    const std::optional<BlockChange> change =
        projectileHits::onHit(world, 1, hitAt(8.5, 65.0, 8.5, registry::BlockFace::Up), true, false, true);
    REQUIRE(change.has_value());
    CHECK(change->x == 8);
    CHECK(change->y == 64);
    CHECK(change->z == 8);
    CHECK(change->stateId == targetOf(15));
    CHECK(world.blockState(8, 64, 8) == targetOf(15));
    CHECK(world.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("target")));
    CHECK(ticksUntilDue(world, 40) == 20);
}

TEST_CASE("anything but an arrow only holds the signal for eight ticks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, targetOf(0)));

    const std::optional<BlockChange> change =
        projectileHits::onHit(world, 1, hitAt(8.0, 65.0, 8.5, registry::BlockFace::Up), false, false, true);
    REQUIRE(change.has_value());
    CHECK(change->stateId == targetOf(1));
    CHECK(ticksUntilDue(world, 40) == 8);
}

TEST_CASE("a target already counting down keeps the signal it has") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, targetOf(0)));
    REQUIRE(
        projectileHits::onHit(world, 1, hitAt(8.5, 65.0, 8.5, registry::BlockFace::Up), true, false, true).has_value());

    // A second hit on the rim would be one, and is ignored while the first is still held.
    CHECK_FALSE(
        projectileHits::onHit(world, 1, hitAt(8.0, 65.0, 8.5, registry::BlockFace::Up), true, false, true).has_value());
    CHECK(world.blockState(8, 64, 8) == targetOf(15));
}

TEST_CASE("the scheduled tick lets a target's signal go, once") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, targetOf(7)));

    const std::optional<BlockChange> released = projectileHits::releaseTarget(world, 8, 64, 8);
    REQUIRE(released.has_value());
    CHECK(released->stateId == targetOf(0));
    CHECK(world.blockState(8, 64, 8) == targetOf(0));

    CHECK_FALSE(projectileHits::releaseTarget(world, 8, 64, 8).has_value());
}

TEST_CASE("only a loaded target releases anything") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::stone().stateId));
    // A block that has a power of its own is still not a target, and keeps it.
    const std::int32_t wire = stateOf("redstone_wire", {{"power", "7"}});
    REQUIRE(world.setBlockState(10, 64, 8, wire));

    CHECK_FALSE(projectileHits::releaseTarget(world, 9, 64, 8).has_value());
    CHECK_FALSE(projectileHits::releaseTarget(world, 10, 64, 8).has_value());
    CHECK(world.blockState(10, 64, 8) == wire);
    CHECK_FALSE(projectileHits::releaseTarget(world, 8, 64, 8).has_value());
    CHECK_FALSE(projectileHits::releaseTarget(world, 4000, 64, 4000).has_value());
}

TEST_CASE("a chorus flower and a decorated pot break where they are hit") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t air = registry::blocks::air().stateId;

    REQUIRE(world.setBlockState(8, 64, 8, stateOf("chorus_flower", {{"age", "3"}})));
    std::optional<BlockChange> change =
        projectileHits::onHit(world, 1, hitAt(8.5, 64.5, 8.0, registry::BlockFace::North), false, false, true);
    REQUIRE(change.has_value());
    CHECK(change->stateId == air);
    CHECK(world.blockState(8, 64, 8) == air);

    REQUIRE(world.setBlockState(8, 64, 8, stateOf("decorated_pot", {{"facing", "north"}})));
    change = projectileHits::onHit(world, 1, hitAt(8.5, 64.5, 8.0, registry::BlockFace::North), true, false, true);
    REQUIRE(change.has_value());
    CHECK(change->stateId == air);
    CHECK(world.blockState(8, 64, 8) == air);
}

TEST_CASE("a big dripleaf tips right over, and only once") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t upright =
        stateOf("big_dripleaf", {{"tilt", "none"}, {"facing", "east"}, {"waterlogged", "true"}});
    REQUIRE(world.setBlockState(8, 64, 8, upright));

    const std::optional<BlockChange> change =
        projectileHits::onHit(world, 1, hitAt(8.5, 65.0, 8.5, registry::BlockFace::Up), false, false, true);
    REQUIRE(change.has_value());
    CHECK(change->stateId != upright);
    CHECK(propertyAt(world, 8, 64, 8, "tilt") == "full");
    // The rest of the state is kept: a hit tips the leaf, it does not turn or drain it.
    CHECK(propertyAt(world, 8, 64, 8, "facing") == "east");
    CHECK(propertyAt(world, 8, 64, 8, "waterlogged") == "true");

    CHECK_FALSE(projectileHits::onHit(world, 1, hitAt(8.5, 65.0, 8.5, registry::BlockFace::Up), false, false, true)
                    .has_value());
}

TEST_CASE("everything else takes a hit and does nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t stone = registry::blocks::stone().stateId;
    REQUIRE(world.setBlockState(8, 64, 8, stone));

    CHECK_FALSE(
        projectileHits::onHit(world, 1, hitAt(8.5, 65.0, 8.5, registry::BlockFace::Up), true, false, true).has_value());
    CHECK(world.blockState(8, 64, 8) == stone);

    // Nothing loaded there, so there is nothing to ask about.
    const BlockHit far{.x = 4000.5, .y = 64.5, .z = 4000.5, .blockX = 4000, .blockY = 64, .blockZ = 4000};
    CHECK_FALSE(projectileHits::onHit(world, 1, far, true, false, true).has_value());
}

TEST_CASE("a flaming arrow lights a candle and a campfire, a plain one does not") {
    // parity: `AbstractCandleBlock.onProjectileHit` and `CampfireBlock.onProjectileHit`, both of
    // which want the projectile alight. Neither was here: only the tnt read that flag.
    const BlockHit side = hitAt(8.5, 64.5, 8.0, registry::BlockFace::North);

    SUBCASE("a candle") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8,
                                    stateOf("candle", {{"candles", "1"}, {"lit", "false"}, {"waterlogged", "false"}})));

        CHECK_FALSE(projectileHits::onHit(world, 1, side, true, false, true).has_value());
        CHECK(propertyAt(world, 8, 64, 8, "lit") == "false");

        REQUIRE(projectileHits::onHit(world, 1, side, true, true, true).has_value());
        CHECK(propertyAt(world, 8, 64, 8, "lit") == "true");
        // Already lit, so a second flaming arrow changes nothing.
        CHECK_FALSE(projectileHits::onHit(world, 1, side, true, true, true).has_value());
    }

    SUBCASE("a candle under water refuses") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8,
                                    stateOf("candle", {{"candles", "1"}, {"lit", "false"}, {"waterlogged", "true"}})));

        CHECK_FALSE(projectileHits::onHit(world, 1, side, true, true, true).has_value());
        CHECK(propertyAt(world, 8, 64, 8, "lit") == "false");
    }

    SUBCASE("a campfire") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(
            8, 64, 8,
            stateOf("campfire",
                    {{"facing", "north"}, {"lit", "false"}, {"signal_fire", "false"}, {"waterlogged", "false"}})));

        CHECK_FALSE(projectileHits::onHit(world, 1, side, true, false, true).has_value());
        REQUIRE(projectileHits::onHit(world, 1, side, true, true, true).has_value());
        CHECK(propertyAt(world, 8, 64, 8, "lit") == "true");
    }
}

TEST_CASE("a bell rings only when it is struck on the side it hangs to face") {
    // parity: `BellBlock.isProperHit`. A floor bell answers along the axis it faces and nothing else,
    // never from above or below, and never on the top fifth of it. A ceiling bell answers from any
    // side. None of this was here: a projectile went straight through a bell.
    const auto bellOf = [](const std::string& attachment) {
        return stateOf("bell", {{"attachment", attachment}, {"facing", "north"}, {"powered", "false"}});
    };

    SUBCASE("a floor bell along its own axis") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8, bellOf("floor")));

        CHECK(projectileHits::onHit(world, 1, hitAt(8.5, 64.5, 8.0, registry::BlockFace::North), true, false, true)
                  .has_value());
        // Across its axis it does not ring.
        CHECK_FALSE(projectileHits::onHit(world, 1, hitAt(8.0, 64.5, 8.5, registry::BlockFace::West), true, false, true)
                        .has_value());
        // From above it never rings.
        CHECK_FALSE(projectileHits::onHit(world, 1, hitAt(8.5, 65.0, 8.5, registry::BlockFace::Up), true, false, true)
                        .has_value());
        // Nor on the top fifth of it.
        CHECK_FALSE(
            projectileHits::onHit(world, 1, hitAt(8.5, 64.9, 8.0, registry::BlockFace::North), true, false, true)
                .has_value());
    }

    SUBCASE("a wall bell across its axis") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8, bellOf("single_wall")));

        CHECK(projectileHits::onHit(world, 1, hitAt(8.0, 64.5, 8.5, registry::BlockFace::West), true, false, true)
                  .has_value());
        CHECK_FALSE(
            projectileHits::onHit(world, 1, hitAt(8.5, 64.5, 8.0, registry::BlockFace::North), true, false, true)
                .has_value());
    }

    SUBCASE("a ceiling bell from any side") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8, bellOf("ceiling")));

        CHECK(projectileHits::onHit(world, 1, hitAt(8.0, 64.5, 8.5, registry::BlockFace::West), true, false, true)
                  .has_value());
        CHECK(projectileHits::onHit(world, 1, hitAt(8.5, 64.5, 8.0, registry::BlockFace::North), true, false, true)
                  .has_value());
    }
}
