#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>

#include <doctest.h>

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

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A stone floor at y=63 over the middle chunk, with everything above it empty.
void floor(WorldState& world) {
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 63, z, stateOf("stone")));
        }
    }
}

/// Runs every fluid tick that comes due until nothing is left, so a test asserts on a world that
/// has finished moving rather than one caught mid-flow.
void settle(const fluidFlow::Context& context, std::int32_t maxTurns = 4000) {
    for (std::int32_t turn = 0; turn < maxTurns; ++turn) {
        const std::vector<std::array<std::int32_t, 3>> due = context.world.dueFluidTicks();
        if (due.empty()) {
            continue;
        }
        for (const std::array<std::int32_t, 3>& position : due) {
            static_cast<void>(fluidFlow::tick(context, position[0], position[1], position[2]));
        }
    }
}

/// The level property of a fluid block, which is zero for a source and eight or more for a fall.
[[nodiscard]] std::int32_t levelAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("level");
    return found == properties.end() ? -1 : std::stoi(found->second);
}

[[nodiscard]] bool isWater(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && fluidFlow::kindOf(*stateId) == FluidKind::Water;
}

struct Fixture {
    WorldState world = emptyWorld();
    GameRuleValues gameRules;
    rng::RandomSource random = rng::RandomSource::legacy(1);

    [[nodiscard]] fluidFlow::Context context(bool ultrawarm = false) {
        return fluidFlow::Context{.world = world, .gameRules = gameRules, .random = random, .ultrawarm = ultrawarm};
    }
};

} // namespace

TEST_CASE("a fluid block is recognised by the states its own block owns") {
    CHECK(fluidFlow::kindOf(stateOf("water")) == FluidKind::Water);
    CHECK(fluidFlow::kindOf(stateOf("lava")) == FluidKind::Lava);
    CHECK_FALSE(fluidFlow::kindOf(air()).has_value());
    CHECK_FALSE(fluidFlow::kindOf(stateOf("stone")).has_value());

    // A waterlogged block carries water too, which is what stops a flow drowning it again.
    const std::optional<std::int32_t> loggedSlab =
        registry::blockStateDefinitions::stateIdOf(basalt::wire::common::Identifier::vanilla("oak_slab"),
                                                   {{"type", "bottom"}, {"waterlogged", "true"}}, std::nullopt);
    REQUIRE(loggedSlab.has_value());
    CHECK(fluidFlow::kindOf(*loggedSlab) == FluidKind::Water);
}

TEST_CASE("all sixteen states of each fluid belong to it, and the next one does not") {
    for (const FluidKind kind : {FluidKind::Water, FluidKind::Lava}) {
        const std::int32_t base = baseStateId(kind);
        for (std::int32_t level = 0; level <= 15; ++level) {
            CHECK(fluidFlow::kindOf(base + level) == kind);
        }
        // The state after the last level is another block, which is what the bound is there for.
        CHECK_FALSE(fluidFlow::kindOf(base + 16) == kind);
    }
}

TEST_CASE("water on any of the five sides lava can flow to stops it, and water below does not") {
    const auto touching = [](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        Fixture fixture;
        floor(fixture.world);
        REQUIRE(fixture.world.setBlockState(8, 65, 8, stateOf("lava")));
        REQUIRE(fixture.world.setBlockState(8 + dx, 65 + dy, 8 + dz, stateOf("water")));
        static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8 + dx, 65 + dy, 8 + dz));
        return *fixture.world.blockState(8, 65, 8);
    };

    CHECK(touching(0, 1, 0) == stateOf("obsidian"));
    CHECK(touching(1, 0, 0) == stateOf("obsidian"));
    CHECK(touching(-1, 0, 0) == stateOf("obsidian"));
    CHECK(touching(0, 0, 1) == stateOf("obsidian"));
    CHECK(touching(0, 0, -1) == stateOf("obsidian"));
    // parity: water under lava is the one direction that does not turn it to rock.
    CHECK(fluidFlow::kindOf(touching(0, -1, 0)) == FluidKind::Lava);
}

TEST_CASE("a flow drains to the same place whatever the delay roll says") {
    // The delay is rolled when a flow settles to a level its neighbours now justify, which is what
    // a cut-off flow spends its time doing.
    const auto roundsToDrain = [](const std::string& fluid, std::int64_t seed) {
        Fixture fixture;
        fixture.random = rng::RandomSource::legacy(seed);
        floor(fixture.world);
        REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf(fluid)));
        static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
        for (std::int32_t turn = 0; turn < 4000; ++turn) {
            for (const std::array<std::int32_t, 3>& position : fixture.world.dueFluidTicks()) {
                static_cast<void>(fluidFlow::tick(fixture.context(), position[0], position[1], position[2]));
            }
        }
        REQUIRE(fixture.world.setBlockState(8, 64, 8, air()));
        static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
        std::int32_t lastMove = 0;
        for (std::int32_t turn = 0; turn < 4000; ++turn) {
            for (const std::array<std::int32_t, 3>& position : fixture.world.dueFluidTicks()) {
                if (!fluidFlow::tick(fixture.context(), position[0], position[1], position[2]).empty()) {
                    lastMove = turn;
                }
            }
        }
        return lastMove;
    };

    // What the roll moves is when, never what: a flow that has finished draining has drained the
    // same way whichever way the roll went, which is why vanilla can afford to roll it on a random
    // with no seed of its own.
    CHECK(roundsToDrain("water", 1) == roundsToDrain("water", 2));
    CHECK(roundsToDrain("lava", 1) == roundsToDrain("lava", 2));
}

TEST_CASE("the last level of a fluid is still a flow, not a source") {
    Fixture fixture;
    floor(fixture.world);
    // Level fifteen is a falling block with one unit in it, the furthest a level can get from zero.
    REQUIRE(fixture.world.setBlockState(8, 64, 8, baseStateId(FluidKind::Water) + 15));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));

    settle(fixture.context());

    // Nothing feeds it, so it drains. Reading it as a source would leave it there for ever.
    CHECK(fixture.world.blockState(8, 64, 8) == air());
}

TEST_CASE("water spreads seven blocks over a flat floor and no further") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));

    settle(fixture.context());

    for (std::int32_t step = 1; step <= 7; ++step) {
        CHECK(levelAt(fixture.world, 8 + step, 64, 8) == step);
        CHECK(levelAt(fixture.world, 8 - step, 64, 8) == step);
        CHECK(levelAt(fixture.world, 8, 64, 8 + step) == step);
    }
    // Eight out is the chunk edge everywhere but north, which is where the flow's limit is inside
    // the floor that was laid.
    CHECK(levelAt(fixture.world, 8, 64, 1) == 7);
    CHECK(fixture.world.blockState(8, 64, 0) == air());
    CHECK(levelAt(fixture.world, 8, 64, 8) == 0);
}

TEST_CASE("overworld lava only reaches three blocks, and the Nether's reaches seven") {
    const auto reach = [](bool ultrawarm) {
        Fixture fixture;
        floor(fixture.world);
        REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("lava")));
        static_cast<void>(fluidFlow::onBlockChanged(fixture.context(ultrawarm), 8, 64, 8));
        settle(fixture.context(ultrawarm));
        std::int32_t found = 0;
        for (std::int32_t step = 1; step <= 7; ++step) {
            if (fixture.world.blockState(8 + step, 64, 8).value_or(air()) != air()) {
                found = step;
            }
        }
        return found;
    };

    // parity: the drop-off is two per block in the overworld and one where lava is fast.
    CHECK(reach(false) == 3);
    CHECK(reach(true) == 7);
}

TEST_CASE("water falls straight down and marks itself falling all the way") {
    Fixture fixture;
    floor(fixture.world);
    for (std::int32_t y = 64; y <= 70; ++y) {
        REQUIRE(fixture.world.setBlockState(8, y, 8, air()));
    }
    REQUIRE(fixture.world.setBlockState(8, 70, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 70, 8));

    settle(fixture.context());

    for (std::int32_t y = 64; y < 70; ++y) {
        CHECK(levelAt(fixture.world, 8, y, 8) == 8);
    }
    CHECK(levelAt(fixture.world, 8, 70, 8) == 0);
    // A source over its own fall still spreads sideways, which is what widens a waterfall: the way
    // down is already water and cannot be replaced, so the sideways branch is the one that runs.
    CHECK(levelAt(fixture.world, 9, 70, 8) == 1);
    CHECK(levelAt(fixture.world, 9, 65, 8) == 8);
}

TEST_CASE("two sources with a gap make a third when the rule allows it") {
    const auto middleOf = [](bool converts) {
        Fixture fixture;
        floor(fixture.world);
        fixture.gameRules.set(gameRules::kWaterSourceConversion, converts ? "true" : "false");
        REQUIRE(fixture.world.setBlockState(7, 64, 8, stateOf("water")));
        REQUIRE(fixture.world.setBlockState(9, 64, 8, stateOf("water")));
        static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 7, 64, 8));
        static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 9, 64, 8));
        settle(fixture.context());
        return levelAt(fixture.world, 8, 64, 8);
    };

    CHECK(middleOf(true) == 0);
    CHECK(middleOf(false) == 1);
}

TEST_CASE("water beside a lava source makes obsidian") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("lava")));
    REQUIRE(fixture.world.setBlockState(9, 64, 8, stateOf("water")));

    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 9, 64, 8));

    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("obsidian"));
}

TEST_CASE("water meeting a lava flow makes cobblestone") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("lava")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
    settle(fixture.context());
    REQUIRE(fixture.world.blockState(11, 64, 8) != air());

    // A water source five blocks past the lava, so the two flows meet without the sources touching.
    REQUIRE(fixture.world.setBlockState(13, 64, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 13, 64, 8));
    settle(fixture.context());

    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("lava"));
    CHECK(fixture.world.blockState(11, 64, 8) == stateOf("cobblestone"));
}

TEST_CASE("lava falling into water turns to stone") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
    REQUIRE(fixture.world.setBlockState(8, 66, 8, stateOf("lava")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 66, 8));

    settle(fixture.context());

    // The lava reaches the water on the way down, so the stone is where the water was.
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("stone"));
    // The lava above it stays where it is, now standing on rock.
    CHECK(fluidFlow::kindOf(*fixture.world.blockState(8, 65, 8)) == FluidKind::Lava);
}

TEST_CASE("lava over soul soil beside blue ice makes basalt") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(8, 63, 8, stateOf("soul_soil")));
    REQUIRE(fixture.world.setBlockState(9, 64, 8, stateOf("blue_ice")));
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("lava")));

    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));

    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("basalt"));
}

TEST_CASE("soul soil without the ice, or ice without the soil, leaves the lava alone") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(8, 63, 8, stateOf("soul_soil")));
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("lava")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
    CHECK(fixture.world.blockState(8, 64, 8) == stateOf("lava"));

    REQUIRE(fixture.world.setBlockState(4, 63, 4, stateOf("stone")));
    REQUIRE(fixture.world.setBlockState(5, 64, 4, stateOf("blue_ice")));
    REQUIRE(fixture.world.setBlockState(4, 64, 4, stateOf("lava")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 4, 64, 4));
    CHECK(fixture.world.blockState(4, 64, 4) == stateOf("lava"));
}

TEST_CASE("a flow drains once its source is gone") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
    settle(fixture.context());
    REQUIRE(isWater(fixture.world, 12, 64, 8));

    REQUIRE(fixture.world.setBlockState(8, 64, 8, air()));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
    settle(fixture.context());

    for (std::int32_t step = 1; step <= 7; ++step) {
        CHECK(fixture.world.blockState(8 + step, 64, 8) == air());
    }
}

TEST_CASE("a flow will not enter a ladder") {
    Fixture fixture;
    floor(fixture.world);
    REQUIRE(fixture.world.setBlockState(9, 64, 8, stateOf("ladder")));
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));

    settle(fixture.context());

    CHECK(fixture.world.blockState(9, 64, 8) == stateOf("ladder"));
    // The other three ways out are unaffected, and the flow reaches past the ladder around it.
    CHECK(isWater(fixture.world, 7, 64, 8));
    CHECK(isWater(fixture.world, 9, 64, 9));
    CHECK(isWater(fixture.world, 10, 64, 8));
}

TEST_CASE("a flow runs at the hole rather than spreading evenly") {
    Fixture fixture;
    floor(fixture.world);
    // A drop three blocks east of where the water lands, and open floor everywhere else.
    REQUIRE(fixture.world.setBlockState(11, 63, 8, air()));
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));

    settle(fixture.context());

    CHECK(isWater(fixture.world, 11, 62, 8));
    // It went the three blocks to the hole before it went anywhere else, so the west side is dry
    // until the source has more than it can send that way.
    CHECK(isWater(fixture.world, 9, 64, 8));
    CHECK(isWater(fixture.world, 10, 64, 8));
}

TEST_CASE("the flow finds the drop whichever way it lies") {
    const auto runsTo = [](std::int32_t dx, std::int32_t dz) {
        Fixture fixture;
        floor(fixture.world);
        REQUIRE(fixture.world.setBlockState(8 + (3 * dx), 63, 8 + (3 * dz), air()));
        REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
        static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
        // Only the first move, before the source has enough to send anywhere else.
        settle(fixture.context(), 8);
        return isWater(fixture.world, 8 + dx, 64, 8 + dz) && !isWater(fixture.world, 8 - dx, 64, 8 - dz);
    };

    CHECK(runsTo(1, 0));
    CHECK(runsTo(-1, 0));
    CHECK(runsTo(0, 1));
    CHECK(runsTo(0, -1));
}

TEST_CASE("a slab under the flow is a floor, not a drop") {
    Fixture fixture;
    floor(fixture.world);
    // The hole three east is plugged by a slab, so the flow has no reason to prefer that way.
    REQUIRE(fixture.world.setBlockState(11, 63, 8, stateOf("oak_slab")));
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));

    settle(fixture.context());

    CHECK(isWater(fixture.world, 5, 64, 8));
    CHECK(isWater(fixture.world, 11, 64, 8));
    CHECK_FALSE(isWater(fixture.world, 11, 62, 8));
}

TEST_CASE("what a flow washed away comes back with the change") {
    Fixture fixture;
    floor(fixture.world);
    const std::int32_t torch = stateOf("torch");
    REQUIRE(fixture.world.setBlockState(9, 64, 8, torch));
    REQUIRE(fixture.world.setBlockState(8, 64, 8, stateOf("water")));

    std::vector<FluidChange> changes;
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
    for (std::int32_t turn = 0; turn < 200; ++turn) {
        for (const std::array<std::int32_t, 3>& position : fixture.world.dueFluidTicks()) {
            for (const FluidChange& change :
                 fluidFlow::tick(fixture.context(), position[0], position[1], position[2])) {
                changes.push_back(change);
            }
        }
    }

    bool sawTorch = false;
    for (const FluidChange& change : changes) {
        if (change.x == 9 && change.y == 64 && change.z == 8) {
            CHECK(change.washedAway == torch);
            CHECK(fluidFlow::kindOf(change.stateId) == FluidKind::Water);
            sawTorch = true;
        }
    }
    CHECK(sawTorch);
}

TEST_CASE("a settled ocean stops asking to be ticked") {
    Fixture fixture;
    floor(fixture.world);
    for (std::int32_t x = 4; x < 13; ++x) {
        for (std::int32_t z = 4; z < 13; ++z) {
            REQUIRE(fixture.world.setBlockState(x, 64, z, stateOf("water")));
        }
    }
    static_cast<void>(fluidFlow::onBlockChanged(fixture.context(), 8, 64, 8));
    settle(fixture.context());

    // Nothing moved, so nothing rewrote a block, so nothing booked another turn.
    std::int32_t moved = 0;
    for (std::int32_t turn = 0; turn < 200; ++turn) {
        for (const std::array<std::int32_t, 3>& position : fixture.world.dueFluidTicks()) {
            moved += static_cast<std::int32_t>(
                fluidFlow::tick(fixture.context(), position[0], position[1], position[2]).size());
        }
    }
    CHECK(moved == 0);
}
