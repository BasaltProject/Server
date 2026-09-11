#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/ScheduledBlocks.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

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
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string pathAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string propertyAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const std::string& name) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A world with room around the origin, so no coordinate a test touches falls outside a chunk.
struct Loaded {
    WorldState state = emptyWorld();
    rng::RandomSource source = rng::RandomSource::legacy(1);

    Loaded() {
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                state.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
    }

    [[nodiscard]] neighbourUpdates::Context context() {
        return neighbourUpdates::Context{.world = state, .worldName = "test", .random = source};
    }

    void put(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        REQUIRE(state.setBlockState(x, y, z, stateId));
    }
};

/// What a run of `afterChange` reported, kept so a test can pin the exact positions.
struct Report {
    std::vector<std::array<std::int32_t, 4>> broken;
    std::vector<std::array<std::int32_t, 4>> reshaped;
    std::vector<BlockChange> effects;

    [[nodiscard]] neighbourUpdates::OnBroken onBroken() {
        return [this](std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) {
            broken.push_back({stateId, x, y, z});
        };
    }

    [[nodiscard]] neighbourUpdates::OnReshaped onReshaped() {
        return [this](std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z) {
            reshaped.push_back({stateId, x, y, z});
        };
    }
};

} // namespace

TEST_CASE("a torch comes down with what held it up") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(8, 65, 8, stateOf("torch"));
    const std::int32_t torch = stateOf("torch");

    loaded.put(8, 64, 8, air());
    Report report;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                  report.onReshaped());

    REQUIRE(report.broken.size() == 1);
    CHECK(report.broken[0] == std::array<std::int32_t, 4>{torch, 8, 65, 8});
    CHECK(loaded.state.blockState(8, 65, 8) == air());
}

TEST_CASE("a cut string fires the line it was part of") {
    // parity: `TripWireBlock.affectNeighborsAfterRemoval`, which the cascade runs for the block that
    // has gone. `updateSource` was written and right, and until this nobody called it on a removal:
    // measured in `cabletrampa`, where cutting the middle string left both hooks loose here and made
    // the jar's line fire.
    Loaded loaded;
    const std::int32_t wire = stateOf("tripwire", {{"attached", "false"}, {"disarmed", "false"}, {"powered", "false"}});
    loaded.put(4, 64, 8, stateOf("tripwire_hook", {{"facing", "east"}, {"attached", "false"}, {"powered", "false"}}));
    loaded.put(5, 64, 8, wire);
    loaded.put(6, 64, 8, wire);
    loaded.put(7, 64, 8, stateOf("tripwire_hook", {{"facing", "west"}, {"attached", "false"}, {"powered", "false"}}));

    loaded.put(5, 64, 8, air());
    std::vector<BlockChange> effects;
    neighbourUpdates::afterChange(loaded.context(), 5, 64, 8, wire, effects, {});

    CHECK(propertyAt(loaded.state, 4, 64, 8, "attached") == "true");
    CHECK(propertyAt(loaded.state, 4, 64, 8, "powered") == "true");
    CHECK(propertyAt(loaded.state, 7, 64, 8, "attached") == "true");
    CHECK(propertyAt(loaded.state, 7, 64, 8, "powered") == "true");
}

TEST_CASE("a cascade with no callbacks at all still breaks what it must") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(8, 65, 8, stateOf("torch"));

    loaded.put(8, 64, 8, air());
    std::vector<BlockChange> effects;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, effects, {});

    CHECK(loaded.state.blockState(8, 65, 8) == air());
}

TEST_CASE("the vertical bounds are the last word on what can break") {
    const std::int32_t torch = stateOf("torch");

    SUBCASE("a block at the ceiling still breaks") {
        Loaded loaded;
        loaded.put(8, 65, 8, torch);
        neighbourUpdates::Context context = loaded.context();
        context.maxY = 65;
        Report report;
        neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, report.effects, report.onBroken());
        CHECK(report.broken.size() == 1);
    }

    SUBCASE("a block above the ceiling is left alone") {
        Loaded loaded;
        loaded.put(8, 65, 8, torch);
        neighbourUpdates::Context context = loaded.context();
        context.maxY = 64;
        Report report;
        neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, report.effects, report.onBroken());
        CHECK(report.broken.empty());
        CHECK(loaded.state.blockState(8, 65, 8) == torch);
    }

    SUBCASE("a block at the floor still breaks") {
        Loaded loaded;
        loaded.put(8, 65, 8, torch);
        neighbourUpdates::Context context = loaded.context();
        context.minY = 65;
        Report report;
        neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, report.effects, report.onBroken());
        CHECK(report.broken.size() == 1);
    }

    SUBCASE("a block below the floor is left alone") {
        Loaded loaded;
        loaded.put(8, 65, 8, torch);
        neighbourUpdates::Context context = loaded.context();
        context.minY = 66;
        Report report;
        neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, report.effects, report.onBroken());
        CHECK(report.broken.empty());
        CHECK(loaded.state.blockState(8, 65, 8) == torch);
    }
}

TEST_CASE("the cascade gives up after a thousand and twenty four neighbours") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("sand"));
    const std::int32_t cactus = stateOf("cactus");
    for (std::int32_t y = 65; y <= 264; ++y) {
        loaded.put(8, y, 8, cactus);
    }

    loaded.put(8, 64, 8, air());
    // The floor is right under the change, so a position out of bounds must be dropped rather than
    // queued: queuing it would eat the budget the cactus above needs.
    neighbourUpdates::Context context = loaded.context();
    context.minY = 64;
    Report report;
    neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, report.effects, report.onBroken());

    // The budget is spent six at a time, so a hundred and seventy one positions come off the queue
    // and each of them takes the cactus above it with it.
    CHECK(report.broken.size() == 171);
    CHECK(loaded.state.blockState(8, 235, 8) == air());
    CHECK(loaded.state.blockState(8, 236, 8) == cactus);
}

TEST_CASE("a fence joins the fence beside it") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("oak_fence"));
    loaded.put(8, 64, 9, stateOf("oak_fence"));

    const neighbourUpdates::Context context = loaded.context();
    const std::optional<std::int32_t> joined =
        neighbourUpdates::connect(context, 8, 64, 8, *loaded.state.blockState(8, 64, 8));

    REQUIRE(joined.has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "south") == "true");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "north") == "false");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "east") == "false");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "west") == "false");
    // Nothing moved the second time, so there is nothing to report.
    CHECK_FALSE(neighbourUpdates::connect(context, 8, 64, 8, *loaded.state.blockState(8, 64, 8)).has_value());
}

TEST_CASE("a fence reaches west and east too") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("oak_fence"));
    loaded.put(7, 64, 8, stateOf("oak_fence"));
    loaded.put(8, 64, 7, stateOf("oak_fence"));

    const neighbourUpdates::Context context = loaded.context();
    REQUIRE(neighbourUpdates::connect(context, 8, 64, 8, *loaded.state.blockState(8, 64, 8)).has_value());

    CHECK(propertyAt(loaded.state, 8, 64, 8, "west") == "true");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "north") == "true");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "east") == "false");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "south") == "false");
}

TEST_CASE("reshaping the block that was written is the caller's choice") {
    const std::int32_t fence = stateOf("oak_fence");

    SUBCASE("on, which is what a hand placing a block wants") {
        Loaded loaded;
        loaded.put(8, 64, 9, stateOf("stone"));
        loaded.put(8, 64, 8, fence);
        Report report;
        neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                      report.onReshaped(), true);
        CHECK(propertyAt(loaded.state, 8, 64, 8, "south") == "true");
        CHECK_FALSE(report.reshaped.empty());
    }

    SUBCASE("off, which is what a command writing one wants") {
        Loaded loaded;
        loaded.put(8, 64, 9, stateOf("stone"));
        loaded.put(8, 64, 8, fence);
        Report report;
        neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                      report.onReshaped(), false);
        CHECK(propertyAt(loaded.state, 8, 64, 8, "south") == "false");
    }
}

TEST_CASE("leaves count their way out from a trunk") {
    Loaded loaded;
    CHECK(neighbourUpdates::leafDistance(loaded.state, 8, 65, 8) == 7);

    loaded.put(8, 64, 8, stateOf("oak_log"));
    CHECK(neighbourUpdates::leafDistance(loaded.state, 8, 65, 8) == 1);

    loaded.put(8, 65, 8, stateOf("oak_leaves", {{"distance", "1"}, {"persistent", "false"}, {"waterlogged", "false"}}));
    CHECK(neighbourUpdates::leafDistance(loaded.state, 8, 66, 8) == 2);

    loaded.put(8, 65, 8, stateOf("oak_leaves", {{"distance", "7"}, {"persistent", "false"}, {"waterlogged", "false"}}));
    CHECK(neighbourUpdates::leafDistance(loaded.state, 8, 66, 8) == 7);

    // A trunk on any of the six faces answers one, which is the early way out of the walk.
    loaded.put(9, 66, 8, stateOf("oak_log"));
    CHECK(neighbourUpdates::leafDistance(loaded.state, 8, 66, 8) == 1);
}

TEST_CASE("leaves take their distance from the trunk that turned up, on their own turn") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("oak_log"));
    loaded.put(8, 65, 8, stateOf("oak_leaves", {{"distance", "7"}, {"persistent", "false"}, {"waterlogged", "false"}}));

    // parity: `LeavesBlock.updateShape` answers the state it was given and books a turn; the distance
    // moves in `LeavesBlock.tick`, one tick later.
    const neighbourUpdates::Context context = loaded.context();
    CHECK_FALSE(neighbourUpdates::connect(context, 8, 65, 8, *loaded.state.blockState(8, 65, 8)).has_value());
    CHECK(propertyAt(loaded.state, 8, 65, 8, "distance") == "7");

    const std::vector<std::array<std::int32_t, 3>> due = loaded.state.dueBlockTicks();
    REQUIRE(due.size() == 1);
    CHECK(due[0] == std::array<std::int32_t, 3>{8, 65, 8});
    REQUIRE(neighbourUpdates::leafTick(loaded.state, 8, 65, 8).has_value());
    CHECK(propertyAt(loaded.state, 8, 65, 8, "distance") == "1");
}

TEST_CASE("a wall works its own shape out, and a caller may answer for it instead") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("cobblestone_wall"));
    const std::int32_t placed = *loaded.state.blockState(8, 64, 8);

    neighbourUpdates::Context context = loaded.context();
    // Nothing around it, and the shape it was placed with is already the right one.
    CHECK_FALSE(neighbourUpdates::connect(context, 8, 64, 8, placed).has_value());

    // With a neighbour to reach out to, the unset hook still settles it: `Walls` answers.
    loaded.put(9, 64, 8, stateOf("stone"));
    const std::optional<std::int32_t> byItself = neighbourUpdates::connect(context, 8, 64, 8, placed);
    REQUIRE(byItself.has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "east") == "low");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "west") == "none");
    loaded.put(9, 64, 8, air());
    loaded.put(8, 64, 8, placed);

    std::vector<std::int32_t> asked;
    context.wallShape = [&asked](std::int32_t stateId, std::int32_t north, std::int32_t south, std::int32_t west,
                                 std::int32_t east,
                                 std::int32_t above) -> std::optional<std::map<std::string, std::string>> {
        asked = {stateId, north, south, west, east, above};
        return std::map<std::string, std::string>{{"north", "low"}, {"up", "false"}};
    };
    loaded.put(8, 64, 7, stateOf("stone"));
    loaded.put(8, 64, 9, stateOf("dirt"));
    loaded.put(7, 64, 8, stateOf("sand"));
    loaded.put(9, 64, 8, stateOf("gravel"));
    loaded.put(8, 65, 8, stateOf("glass"));

    const std::optional<std::int32_t> rebuilt = neighbourUpdates::connect(context, 8, 64, 8, placed);
    REQUIRE(rebuilt.has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "north") == "low");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "up") == "false");
    // The five neighbours reach the wall in the order it names them, which is easy to swap and
    // impossible to see once it is wrong.
    CHECK(asked == std::vector<std::int32_t>{placed, stateOf("stone"), stateOf("dirt"), stateOf("sand"),
                                             stateOf("gravel"), stateOf("glass")});
}

TEST_CASE("the dust is asked to redraw itself where it sits") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(8, 65, 8, stateOf("redstone_wire"));

    neighbourUpdates::Context context = loaded.context();
    std::vector<BlockChange> bubbles;
    // The unset hook is `RedstoneWire` itself, and a lone dust on stone is already a dot.
    CHECK_FALSE(neighbourUpdates::reshape(context, 8, 65, 8, bubbles).has_value());

    std::vector<std::int32_t> asked;
    const std::int32_t drawn = stateOf(
        "redstone_wire", {{"north", "side"}, {"south", "side"}, {"east", "none"}, {"west", "none"}, {"power", "0"}});
    context.wireReshape = [&asked, drawn](WorldState&, std::int32_t x, std::int32_t y, std::int32_t z,
                                          std::int32_t stateId,
                                          std::optional<registry::BlockFace>) -> std::optional<std::int32_t> {
        asked = {x, y, z, stateId};
        return drawn;
    };
    const std::optional<std::int32_t> redrawn = neighbourUpdates::reshape(context, 8, 65, 8, bubbles);
    REQUIRE(redrawn.has_value());
    CHECK(*redrawn == drawn);
    CHECK(asked == std::vector<std::int32_t>{8, 65, 8, stateOf("redstone_wire")});
}

TEST_CASE("a dispenser is asked about the change and about every face of it") {
    Loaded loaded;
    neighbourUpdates::Context context = loaded.context();
    std::vector<std::array<std::int32_t, 3>> asked;
    context.dispenser = [&asked](WorldState&, std::int32_t x, std::int32_t y,
                                 std::int32_t z) -> std::optional<BlockChange> {
        asked.push_back({x, y, z});
        return std::nullopt;
    };

    std::vector<BlockChange> effects;
    neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, effects, {});

    // parity: `NeighborUpdater.UPDATE_ORDER`, west, east, down, up, north and south, and the square
    // itself at the end.
    REQUIRE(asked.size() == 7);
    CHECK(asked[0] == std::array<std::int32_t, 3>{7, 64, 8});
    CHECK(asked[1] == std::array<std::int32_t, 3>{9, 64, 8});
    CHECK(asked[2] == std::array<std::int32_t, 3>{8, 63, 8});
    CHECK(asked[3] == std::array<std::int32_t, 3>{8, 65, 8});
    CHECK(asked[4] == std::array<std::int32_t, 3>{8, 64, 7});
    CHECK(asked[5] == std::array<std::int32_t, 3>{8, 64, 9});
    CHECK(asked[6] == std::array<std::int32_t, 3>{8, 64, 8});
}

TEST_CASE("what a dispenser answers is reported like any other change") {
    Loaded loaded;
    neighbourUpdates::Context context = loaded.context();
    context.dispenser = [](WorldState&, std::int32_t x, std::int32_t y, std::int32_t z) -> std::optional<BlockChange> {
        if (x != 8 || y != 64 || z != 8) {
            return std::nullopt;
        }
        return BlockChange{.x = x, .y = y, .z = z, .stateId = 77};
    };

    Report report;
    neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                  report.onReshaped());

    REQUIRE(report.reshaped.size() == 1);
    CHECK(report.reshaped[0] == std::array<std::int32_t, 4>{77, 8, 64, 8});
}

TEST_CASE("a block that can fall books its two ticks, and so does an egg its hatch") {
    Loaded loaded;
    loaded.put(8, 63, 8, stateOf("stone"));
    loaded.put(8, 64, 8, stateOf("sniffer_egg"));
    loaded.put(8, 65, 8, stateOf("sand"));

    std::vector<BlockChange> effects;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, effects, {});

    // parity: the turn is booked under the block that books it, `sand` and not the entity type.
    CHECK(loaded.state.hasScheduledBlockTick(8, 65, 8, basalt::wire::common::Identifier::vanilla("sand")));
    CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::snifferEggTickType()));
}

TEST_CASE("frogspawn books its own long wait") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("frogspawn"));

    std::vector<BlockChange> effects;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, effects, {});

    CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, scheduledBlocks::frogspawnTickType()));
}

TEST_CASE("a sponge drinks what is around it the moment it lands") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("sponge"));
    loaded.put(8, 65, 8, stateOf("water"));
    loaded.put(8, 64, 9, stateOf("water"));

    Report report;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                  report.onReshaped());

    CHECK(pathAt(loaded.state, 8, 64, 8) == "wet_sponge");
    CHECK(loaded.state.blockState(8, 65, 8) == air());
    CHECK(loaded.state.blockState(8, 64, 9) == air());
}

TEST_CASE("a wet sponge dries out where water boils") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("wet_sponge"));
    neighbourUpdates::Context context = loaded.context();

    SUBCASE("in a world that keeps its water") {
        std::vector<BlockChange> effects;
        neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, effects, {});
        CHECK(pathAt(loaded.state, 8, 64, 8) == "wet_sponge");
    }

    SUBCASE("in one that does not") {
        context.evaporates = true;
        std::vector<BlockChange> effects;
        neighbourUpdates::afterChange(context, 8, 64, 8, std::nullopt, effects, {});
        CHECK(pathAt(loaded.state, 8, 64, 8) == "sponge");
    }
}

TEST_CASE("a lamp lights when the change powers it") {
    Loaded loaded;
    loaded.put(8, 65, 8, stateOf("redstone_lamp"));
    loaded.put(8, 64, 8, stateOf("redstone_block"));

    Report report;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                  report.onReshaped());

    CHECK(propertyAt(loaded.state, 8, 65, 8, "lit") == "true");
}

TEST_CASE("a covered patch of farmland gives way to dirt rather than breaking") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("farmland"));
    loaded.put(8, 65, 8, stateOf("stone"));

    neighbourUpdates::Context context = loaded.context();
    std::vector<BlockChange> bubbles;
    const std::optional<std::int32_t> turned = neighbourUpdates::reshape(context, 8, 64, 8, bubbles);

    REQUIRE(turned.has_value());
    CHECK(pathAt(loaded.state, 8, 64, 8) == "dirt");
}

TEST_CASE("nothing is reshaped where there is no chunk") {
    Loaded loaded;
    neighbourUpdates::Context context = loaded.context();
    std::vector<BlockChange> bubbles;
    CHECK_FALSE(neighbourUpdates::reshape(context, 4000, 64, 4000, bubbles).has_value());
}

TEST_CASE("everything the block held up comes down, whichever side it was on") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(8, 65, 8, stateOf("torch"));
    loaded.put(8, 63, 8, stateOf("lantern", {{"hanging", "true"}, {"waterlogged", "false"}}));
    loaded.put(8, 64, 7, stateOf("wall_torch", {{"facing", "north"}}));
    loaded.put(8, 64, 9, stateOf("wall_torch", {{"facing", "south"}}));
    loaded.put(7, 64, 8, stateOf("wall_torch", {{"facing", "west"}}));
    loaded.put(9, 64, 8, stateOf("wall_torch", {{"facing", "east"}}));

    loaded.put(8, 64, 8, air());
    Report report;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken());

    // Pinned in order, not merely counted: a neighbour reached the long way round still comes down,
    // so only the order says the six faces are the six faces. parity:
    // `BlockBehaviour.UPDATE_SHAPE_ORDER`, west first and up last.
    std::vector<std::array<std::int32_t, 3>> where;
    for (const std::array<std::int32_t, 4>& gone : report.broken) {
        where.push_back({gone[1], gone[2], gone[3]});
    }
    CHECK(where == std::vector<std::array<std::int32_t, 3>>{
                       {7, 64, 8}, {9, 64, 8}, {8, 64, 7}, {8, 64, 9}, {8, 63, 8}, {8, 65, 8}});
}

TEST_CASE("a block that only changed shape carries the walk on to the next one") {
    Loaded loaded;
    const std::int32_t fence = stateOf("oak_fence");
    for (std::int32_t z = 8; z <= 11; ++z) {
        loaded.put(8, 64, z, fence);
    }

    Report report;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                  report.onReshaped());

    // The last fence of the row is three steps from the change and only reachable through the two
    // that merely reshaped on the way.
    CHECK(propertyAt(loaded.state, 8, 64, 11, "north") == "true");
    CHECK(propertyAt(loaded.state, 8, 64, 10, "north") == "true");
    CHECK(propertyAt(loaded.state, 8, 64, 10, "south") == "true");
}

TEST_CASE("a staircase turns its corner towards the flight it meets") {
    const std::int32_t straight = stateOf(
        "oak_stairs", {{"facing", "north"}, {"half", "bottom"}, {"shape", "straight"}, {"waterlogged", "false"}});
    const std::int32_t crossing = stateOf(
        "oak_stairs", {{"facing", "east"}, {"half", "bottom"}, {"shape", "straight"}, {"waterlogged", "false"}});

    SUBCASE("the flight in front of it makes an outer corner") {
        Loaded loaded;
        loaded.put(8, 64, 8, straight);
        loaded.put(8, 64, 7, crossing);
        REQUIRE(neighbourUpdates::connect(loaded.context(), 8, 64, 8, straight).has_value());
        CHECK(propertyAt(loaded.state, 8, 64, 8, "shape") == "outer_right");
    }

    SUBCASE("the flight behind it makes an inner one") {
        Loaded loaded;
        loaded.put(8, 64, 8, straight);
        loaded.put(8, 64, 9, crossing);
        REQUIRE(neighbourUpdates::connect(loaded.context(), 8, 64, 8, straight).has_value());
        CHECK(propertyAt(loaded.state, 8, 64, 8, "shape") == "inner_right");
    }

    SUBCASE("a flight running east of it keeps it straight") {
        Loaded loaded;
        loaded.put(8, 64, 8, straight);
        loaded.put(8, 64, 9, crossing);
        loaded.put(9, 64, 8, straight);
        CHECK_FALSE(neighbourUpdates::connect(loaded.context(), 8, 64, 8, straight).has_value());
        CHECK(propertyAt(loaded.state, 8, 64, 8, "shape") == "straight");
    }

    SUBCASE("a flight running alongside it keeps it straight") {
        Loaded loaded;
        loaded.put(8, 64, 8, straight);
        loaded.put(8, 64, 7, crossing);
        loaded.put(7, 64, 8, straight);
        CHECK_FALSE(neighbourUpdates::connect(loaded.context(), 8, 64, 8, straight).has_value());
        CHECK(propertyAt(loaded.state, 8, 64, 8, "shape") == "straight");
    }
}

TEST_CASE("a column of bubbles carries the walk on as much as a block that moved") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("soul_sand"));
    for (std::int32_t y = 65; y <= 68; ++y) {
        loaded.put(8, y, 8, stateOf("water"));
    }
    // Only reachable through the water the column climbed: nothing else beside the change moves. The
    // marker is a dust and not a torch because a torch checks its footing in `updateShape`, which is
    // gated on the news coming from below, and the dust checks it in `neighborChanged`, which arrives
    // from every side.
    loaded.put(7, 65, 8, stateOf("redstone_wire"));

    Report report;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken(),
                                  report.onReshaped());

    CHECK(pathAt(loaded.state, 8, 65, 8) == "bubble_column");
    CHECK(pathAt(loaded.state, 8, 68, 8) == "bubble_column");
    CHECK(loaded.state.blockState(7, 65, 8) == air());
}

TEST_CASE("a lone neighbour east of the change is reached, and nothing beyond it") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));
    loaded.put(9, 64, 8, stateOf("wall_torch", {{"facing", "east"}}));
    loaded.put(11, 64, 8, stateOf("wall_torch", {{"facing", "east"}}));

    loaded.put(8, 64, 8, air());
    Report report;
    neighbourUpdates::afterChange(loaded.context(), 8, 64, 8, std::nullopt, report.effects, report.onBroken());

    // The torch two blocks out is unsupported too, and stays up: the walk only ever steps one.
    REQUIRE(report.broken.size() == 1);
    CHECK(report.broken[0][1] == 9);
    CHECK(loaded.state.blockState(11, 64, 8) != air());
}

TEST_CASE("a signal arriving lights a tnt, and the game rule is what stops it") {
    // parity: `TntBlock.neighborChanged` into `prime`. The cascade is the only path a redstone block
    // put down by hand or by command takes, and it used to hand the reactors a hardcoded false for
    // the `tnt_explodes` rule, so nothing a player could build ever lit one.
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("tnt", {{"unstable", "false"}}));
    loaded.put(9, 64, 8, stateOf("redstone_block"));

    Report report;
    neighbourUpdates::afterChange(loaded.context(), 9, 64, 8, std::nullopt, report.effects, report.onBroken());
    CHECK(loaded.state.blockState(8, 64, 8) == air());
    CHECK(loaded.state.takePrimedTnt().size() == 1);

    loaded.put(8, 64, 8, stateOf("tnt", {{"unstable", "false"}}));
    neighbourUpdates::Context off = loaded.context();
    off.tntExplodes = false;
    neighbourUpdates::afterChange(off, 9, 64, 8, std::nullopt, report.effects, report.onBroken());
    CHECK(loaded.state.blockState(8, 64, 8) != air());
    CHECK(loaded.state.takePrimedTnt().empty());
}
