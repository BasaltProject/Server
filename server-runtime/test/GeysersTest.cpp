#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

constexpr std::int64_t kSeed = 1234567;

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

[[nodiscard]] std::int32_t sulfur(const std::string& state) {
    return stateOf("potent_sulfur", {{"potent_sulfur_state", state}});
}

[[nodiscard]] std::int32_t stillWater() { return stateOf("water", {{"level", "0"}}); }

[[nodiscard]] std::int32_t flowingWater() { return stateOf("water", {{"level", "1"}}); }

[[nodiscard]] std::string stateAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    REQUIRE(stateId.has_value());
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("potent_sulfur_state");
    return found == properties.end() ? std::string{} : found->second;
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

    /// Sulfur at (8, 64, 8) with `below` under it and `depth` blocks of still water over it.
    void geyser(const std::string& sulfurState, std::int32_t below, std::int32_t depth) {
        put(8, 63, 8, below);
        put(8, 64, 8, sulfur(sulfurState));
        for (std::int32_t above = 1; above <= depth; ++above) {
            put(8, 64 + above, 8, stillWater());
        }
    }
};

} // namespace

TEST_CASE("sulfur out of water is dry, whatever is under it") {
    Loaded loaded;
    loaded.geyser("wet", stateOf("magma_block"), 0);

    const std::optional<std::int32_t> turned = geysers::validState(loaded.state, kSeed, 0, 8, 64, 8, sulfur("wet"));

    REQUIRE(turned.has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "dry");
}

TEST_CASE("sulfur under still water over nothing in particular is merely wet") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("stone"), 2);

    REQUIRE(geysers::validState(loaded.state, kSeed, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "wet");
}

TEST_CASE("lava under it makes it erupt without stopping") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("lava", {{"level", "0"}}), 2);

    REQUIRE(geysers::validState(loaded.state, kSeed, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "continuous");

    // parity: flowing lava is not a source, so it drives nothing.
    Loaded flowing;
    flowing.geyser("dry", stateOf("lava", {{"level", "1"}}), 2);
    REQUIRE(geysers::validState(flowing.state, kSeed, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(flowing.state, 8, 64, 8) == "wet");
}

TEST_CASE("a magma block under it makes it keep a clock, starting dormant") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("magma_block"), 2);

    REQUIRE(geysers::validState(loaded.state, kSeed, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "dormant");

    // One that was already erupting keeps erupting rather than resetting.
    Loaded erupting;
    erupting.geyser("erupting", stateOf("magma_block"), 2);
    CHECK_FALSE(geysers::validState(erupting.state, kSeed, 0, 8, 64, 8, sulfur("erupting")).has_value());
    CHECK(stateAt(erupting.state, 8, 64, 8) == "erupting");
}

TEST_CASE("a state that is already right is left alone") {
    Loaded loaded;
    loaded.geyser("wet", stateOf("stone"), 2);

    CHECK_FALSE(geysers::validState(loaded.state, kSeed, 0, 8, 64, 8, sulfur("wet")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "wet");
    // And a block that is not sulfur has no state to settle.
    CHECK_FALSE(geysers::validState(loaded.state, kSeed, 0, 8, 64, 8, stateOf("stone")).has_value());
}

TEST_CASE("taking a clock on books a turn, and losing it does not") {
    Loaded taking;
    taking.geyser("dry", stateOf("magma_block"), 2);
    REQUIRE(geysers::validState(taking.state, kSeed, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(taking.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));

    Loaded losing;
    losing.geyser("dormant", stateOf("stone"), 2);
    REQUIRE(geysers::validState(losing.state, kSeed, 0, 8, 64, 8, sulfur("dormant")).has_value());
    CHECK(stateAt(losing.state, 8, 64, 8) == "wet");
    CHECK_FALSE(losing.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));
}

TEST_CASE("the turn lands on a twenty-tick boundary, whatever tick it was booked on") {
    for (const std::int64_t gameTime : {std::int64_t{0}, std::int64_t{1}, std::int64_t{19}, std::int64_t{137}}) {
        Loaded loaded;
        loaded.geyser("dry", stateOf("magma_block"), 2);
        geysers::scheduleTurn(loaded.state, kSeed, gameTime, 8, 64, 8, "dormant");

        // The delay is counted from now, so the tick it lands on is a multiple of twenty.
        std::int64_t elapsed = 0;
        while (loaded.state.dueBlockTicks().empty() && elapsed < 100000) {
            ++elapsed;
        }
        REQUIRE(elapsed < 100000);
        CHECK((gameTime + elapsed + 1) % 20 == 0);
    }
}

TEST_CASE("a deeper geyser waits longer to erupt") {
    const auto delayOf = [](std::int32_t depth) {
        Loaded loaded;
        loaded.geyser("dry", stateOf("magma_block"), depth);
        geysers::scheduleTurn(loaded.state, kSeed, 0, 8, 64, 8, "dormant");
        std::int64_t elapsed = 0;
        while (loaded.state.dueBlockTicks().empty() && elapsed < 100000) {
            ++elapsed;
        }
        return elapsed;
    };

    // parity: ten twenty-tick steps for every block of water over the first one.
    CHECK(delayOf(3) - delayOf(2) == 200);
    CHECK(delayOf(4) - delayOf(3) == 200);
}

TEST_CASE("two geysers side by side keep their own rhythm, and the same one every time") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("magma_block"), 2);
    loaded.put(9, 63, 8, stateOf("magma_block"));
    loaded.put(9, 64, 8, sulfur("dormant"));
    loaded.put(9, 65, 8, stillWater());
    loaded.put(9, 66, 8, stillWater());

    geysers::scheduleTurn(loaded.state, kSeed, 0, 8, 64, 8, "dormant");
    geysers::scheduleTurn(loaded.state, kSeed, 0, 9, 64, 8, "dormant");

    std::vector<std::array<std::int32_t, 3>> order;
    for (std::int64_t elapsed = 0; elapsed < 100000 && order.size() < 2; ++elapsed) {
        for (const std::array<std::int32_t, 3>& due : loaded.state.dueBlockTicks()) {
            order.push_back(due);
        }
    }
    REQUIRE(order.size() == 2);
    // Different columns, different waits.
    CHECK(order[0] != order[1]);

    // And the same seed gives the same wait again.
    Loaded again;
    again.geyser("dry", stateOf("magma_block"), 2);
    geysers::scheduleTurn(again.state, kSeed, 0, 8, 64, 8, "dormant");
    std::int64_t first = 0;
    while (again.state.dueBlockTicks().empty() && first < 100000) {
        ++first;
    }
    Loaded thrice;
    thrice.geyser("dry", stateOf("magma_block"), 2);
    geysers::scheduleTurn(thrice.state, kSeed, 0, 8, 64, 8, "dormant");
    std::int64_t second = 0;
    while (thrice.state.dueBlockTicks().empty() && second < 100000) {
        ++second;
    }
    CHECK(first == second);
}

TEST_CASE("the turn flips dormant to erupting and back") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 2);
    std::vector<BlockChange> changes;

    geysers::tick(loaded.state, kSeed, 0, 8, 64, 8, changes);

    REQUIRE(changes.size() == 1);
    CHECK(changes[0].x == 8);
    CHECK(changes[0].y == 64);
    CHECK(changes[0].z == 8);
    CHECK(stateAt(loaded.state, 8, 64, 8) == "erupting");
    // And the next one books itself.
    CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));

    geysers::tick(loaded.state, kSeed, 0, 8, 64, 8, changes);
    CHECK(changes.size() == 2);
    CHECK(stateAt(loaded.state, 8, 64, 8) == "dormant");
}

TEST_CASE("only a geyser with a clock and a way out flips") {
    std::vector<BlockChange> changes;

    Loaded wet;
    wet.geyser("wet", stateOf("stone"), 2);
    geysers::tick(wet.state, kSeed, 0, 8, 64, 8, changes);
    CHECK(changes.empty());

    Loaded plain;
    plain.put(8, 64, 8, stateOf("stone"));
    geysers::tick(plain.state, kSeed, 0, 8, 64, 8, changes);
    CHECK(changes.empty());

    // A geyser under a lid has nowhere for the plume to go, so it does not flip either.
    Loaded capped;
    capped.geyser("dormant", stateOf("magma_block"), 2);
    capped.put(8, 67, 8, stateOf("stone"));
    geysers::tick(capped.state, kSeed, 0, 8, 64, 8, changes);
    CHECK(changes.empty());
    CHECK(stateAt(capped.state, 8, 64, 8) == "dormant");
}

TEST_CASE("more than four blocks of water over it is too deep to reach the top") {
    Loaded deep;
    deep.geyser("dormant", stateOf("magma_block"), 6);
    std::vector<BlockChange> changes;

    geysers::tick(deep.state, kSeed, 0, 8, 64, 8, changes);

    // parity: `ALLOWED_WATER_BLOCKS_ABOVE`, four and no more.
    CHECK(changes.empty());

    Loaded shallow;
    shallow.geyser("dormant", stateOf("magma_block"), 4);
    geysers::tick(shallow.state, kSeed, 0, 8, 64, 8, changes);
    CHECK(changes.size() == 1);
}

TEST_CASE("flowing water over it is not water it can rise through") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 1);
    loaded.put(8, 66, 8, flowingWater());
    std::vector<BlockChange> changes;

    geysers::tick(loaded.state, kSeed, 0, 8, 64, 8, changes);

    // The flowing block is not a source, so the plume stops there and comes out of it.
    CHECK(changes.size() == 1);
}

namespace {

/// The tick a geyser at (8, 64, 8) is booked for, or -1 when nothing was booked.
[[nodiscard]] std::int64_t bookedDelay(WorldState& world) {
    for (std::int64_t elapsed = 0; elapsed < 100000; ++elapsed) {
        if (!world.dueBlockTicks().empty()) {
            return elapsed;
        }
    }
    return -1;
}

/// The same draw the geyser makes, from the same seed at the same place.
[[nodiscard]] basalt::server::random::RandomSource geyserRandom(std::int64_t seed, std::int32_t x, std::int32_t y,
                                                                std::int32_t z) {
    basalt::server::random::RandomSource root = basalt::server::random::RandomSource::xoroshiro(seed ^ -904011478LL);
    return root.forkPositional().at(x, y, z);
}

} // namespace

TEST_CASE("the wait is ten steps per block of water plus a roll of fifteen to thirty") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("magma_block"), 2);
    geysers::scheduleTurn(loaded.state, kSeed, 0, 8, 64, 8, "dormant");

    // parity: `SERVER_WAITING_COUNTDOWN_TICKER`'s own arithmetic, derived rather than remembered.
    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    const std::int32_t steps = (10 * (2 - 1)) + source.nextIntBetweenInclusive(15, 30);
    CHECK(bookedDelay(loaded.state) == 20 + ((steps - 1) * 20) - 1);
}

TEST_CASE("an erupting geyser waits a roll of one or two, after a draw it throws away") {
    Loaded loaded;
    loaded.geyser("erupting", stateOf("magma_block"), 3);
    geysers::scheduleTurn(loaded.state, kSeed, 0, 8, 64, 8, "erupting");

    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    // parity: the erupting branch draws one it never reads, which is what keeps the two apart.
    static_cast<void>(source.nextInt());
    const std::int32_t steps = 3 - 1 + source.nextIntBetweenInclusive(1, 2);
    CHECK(bookedDelay(loaded.state) == 20 + ((steps - 1) * 20) - 1);
}

TEST_CASE("a geyser with no water over it books nothing at all") {
    Loaded loaded;
    loaded.put(8, 63, 8, stateOf("magma_block"));
    loaded.put(8, 64, 8, sulfur("erupting"));

    geysers::scheduleTurn(loaded.state, kSeed, 0, 8, 64, 8, "erupting");

    // parity: no blocks of water is a wait of nothing, and a wait of nothing is not booked.
    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    static_cast<void>(source.nextInt());
    const std::int32_t steps = 0 - 1 + source.nextIntBetweenInclusive(1, 2);
    if (steps <= 0) {
        CHECK_FALSE(loaded.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));
    } else {
        CHECK(loaded.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));
    }
}

TEST_CASE("exactly four blocks of water reach the top, and five do not") {
    for (std::int32_t depth = 1; depth <= 5; ++depth) {
        Loaded loaded;
        loaded.geyser("dormant", stateOf("magma_block"), depth);
        std::vector<BlockChange> changes;
        geysers::tick(loaded.state, kSeed, 0, 8, 64, 8, changes);
        // parity: `ALLOWED_WATER_BLOCKS_ABOVE` is four, so the fifth block of water caps it.
        CHECK(changes.size() == (depth <= 4 ? 1U : 0U));
    }
}

TEST_CASE("only the block directly over it decides whether it is under water") {
    Loaded gap;
    gap.put(8, 63, 8, stateOf("magma_block"));
    gap.put(8, 64, 8, sulfur("wet"));
    // Water one higher, with air in between.
    gap.put(8, 66, 8, stillWater());

    REQUIRE(geysers::validState(gap.state, kSeed, 0, 8, 64, 8, sulfur("wet")).has_value());
    CHECK(stateAt(gap.state, 8, 64, 8) == "dry");
}

TEST_CASE("a waterlogged block counts as the water over it") {
    Loaded logged;
    logged.put(8, 63, 8, stateOf("magma_block"));
    logged.put(8, 64, 8, sulfur("dry"));
    logged.put(8, 65, 8, stateOf("oak_slab", {{"type", "bottom"}, {"waterlogged", "true"}}));

    REQUIRE(geysers::validState(logged.state, kSeed, 0, 8, 64, 8, sulfur("dry")).has_value());
    // parity: `isSourceIfFluid` asks the fluid, not the block, so a waterlogged slab is water.
    CHECK(stateAt(logged.state, 8, 64, 8) == "dormant");
    // The slab itself caps the plume, though, so there is no turn to book.
    CHECK_FALSE(logged.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));
}

TEST_CASE("the plume comes out through anything it can pass, not only through air") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 1);
    loaded.put(8, 66, 8, stateOf("torch"));
    std::vector<BlockChange> changes;

    geysers::tick(loaded.state, kSeed, 0, 8, 64, 8, changes);

    // parity: `isGeyserPassableBlock`, which a torch is and a slab is not.
    CHECK(changes.size() == 1);

    Loaded capped;
    capped.geyser("dormant", stateOf("magma_block"), 1);
    capped.put(8, 66, 8, stateOf("oak_slab", {{"type", "bottom"}, {"waterlogged", "false"}}));
    std::vector<BlockChange> none;
    geysers::tick(capped.state, kSeed, 0, 8, 64, 8, none);
    CHECK(none.empty());
}

TEST_CASE("a geyser outside the loaded world settles nothing") {
    Loaded loaded;

    CHECK_FALSE(geysers::validState(loaded.state, kSeed, 0, 500, 64, 500, sulfur("wet")).has_value());
    std::vector<BlockChange> changes;
    geysers::tick(loaded.state, kSeed, 0, 500, 64, 500, changes);
    CHECK(changes.empty());
}

TEST_CASE("the same arithmetic holds at every column, whatever the roll comes out as") {
    for (std::int32_t x = 2; x <= 13; ++x) {
        Loaded dormant;
        dormant.put(x, 63, 8, stateOf("magma_block"));
        dormant.put(x, 64, 8, sulfur("dormant"));
        dormant.put(x, 65, 8, stillWater());
        dormant.put(x, 66, 8, stillWater());
        geysers::scheduleTurn(dormant.state, kSeed, 0, x, 64, 8, "dormant");
        basalt::server::random::RandomSource waiting = geyserRandom(kSeed, x, 64, 8);
        const std::int32_t waitSteps = (10 * (2 - 1)) + waiting.nextIntBetweenInclusive(15, 30);
        CHECK(bookedDelay(dormant.state) == 20 + ((waitSteps - 1) * 20) - 1);

        Loaded erupting;
        erupting.put(x, 63, 8, stateOf("magma_block"));
        erupting.put(x, 64, 8, sulfur("erupting"));
        erupting.put(x, 65, 8, stillWater());
        erupting.put(x, 66, 8, stillWater());
        geysers::scheduleTurn(erupting.state, kSeed, 0, x, 64, 8, "erupting");
        basalt::server::random::RandomSource going = geyserRandom(kSeed, x, 64, 8);
        static_cast<void>(going.nextInt());
        const std::int32_t goingSteps = 2 - 1 + going.nextIntBetweenInclusive(1, 2);
        CHECK(bookedDelay(erupting.state) == 20 + ((goingSteps - 1) * 20) - 1);
    }
}

TEST_CASE("a wait of one step is still booked, and a wait of none is not") {
    // With no water over it the wait is the roll less one, so it is either one step or none.
    bool sawOne = false;
    bool sawNone = false;
    for (std::int32_t x = 2; x <= 13 && !(sawOne && sawNone); ++x) {
        basalt::server::random::RandomSource source = geyserRandom(kSeed, x, 64, 8);
        static_cast<void>(source.nextInt());
        const std::int32_t roll = source.nextIntBetweenInclusive(1, 2);

        Loaded loaded;
        loaded.put(x, 63, 8, stateOf("magma_block"));
        loaded.put(x, 64, 8, sulfur("erupting"));
        geysers::scheduleTurn(loaded.state, kSeed, 0, x, 64, 8, "erupting");
        CHECK(loaded.state.hasScheduledBlockTick(x, 64, 8, geysers::block()) == (roll > 1));
        sawOne = sawOne || roll > 1;
        sawNone = sawNone || roll == 1;
    }
    CHECK(sawOne);
    CHECK(sawNone);
}

TEST_CASE("water in a block the plume can pass through is water it keeps rising through") {
    Loaded loaded;
    loaded.put(8, 63, 8, stateOf("magma_block"));
    loaded.put(8, 64, 8, sulfur("dormant"));
    loaded.put(8, 65, 8, stillWater());
    // Glow lichen holds nothing up and can be waterlogged, so the plume goes straight through it.
    loaded.put(8, 66, 8,
               stateOf("glow_lichen", {{"down", "true"},
                                       {"east", "false"},
                                       {"north", "false"},
                                       {"south", "false"},
                                       {"up", "false"},
                                       {"west", "false"},
                                       {"waterlogged", "true"}}));
    geysers::scheduleTurn(loaded.state, kSeed, 0, 8, 64, 8, "dormant");

    // Two blocks of water, not one: the lichen counts as water and is passed rather than stopped at.
    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    const std::int32_t steps = (10 * (2 - 1)) + source.nextIntBetweenInclusive(15, 30);
    CHECK(bookedDelay(loaded.state) == 20 + ((steps - 1) * 20) - 1);
}
