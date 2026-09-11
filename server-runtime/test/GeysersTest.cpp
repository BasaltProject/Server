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
        withBlockEntity(8, 64, 8);
        for (std::int32_t above = 1; above <= depth; ++above) {
            put(8, 64 + above, 8, stillWater());
        }
    }

    /// The block entity the wait lives in, which a real write gets from `blockEntityDefaults`.
    void withBlockEntity(std::int32_t x, std::int32_t y, std::int32_t z) {
        basalt::server::world::Chunk* chunk = state.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        basalt::wire::nbt::NbtCompound data;
        data.putInt("countdown", -1);
        chunk->setBlockEntity(
            basalt::server::world::ChunkBlockEntity{x & 15, y, z & 15, geysers::block(), std::move(data)});
    }

    /// What the geyser's wait says now.
    [[nodiscard]] std::int32_t countdown(std::int32_t x = 8, std::int32_t y = 64, std::int32_t z = 8) {
        basalt::server::world::Chunk* chunk = state.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        const basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
        REQUIRE(entity != nullptr);
        return entity->data().getInt("countdown", -1);
    }

    /// Turns of the ticker, twenty ticks apart, until the geyser flips or `limit` runs out.
    [[nodiscard]] std::int32_t stepsToFlip(std::int32_t limit = 10000) {
        for (std::int32_t step = 1; step <= limit; ++step) {
            if (geysers::countdownTick(state, kSeed, static_cast<std::int64_t>(step) * 20, 8, 64, 8).has_value()) {
                return step;
            }
        }
        return -1;
    }
};

} // namespace

TEST_CASE("sulfur out of water is dry, whatever is under it") {
    Loaded loaded;
    loaded.geyser("wet", stateOf("magma_block"), 0);

    const std::optional<std::int32_t> turned = geysers::validState(loaded.state, 0, 8, 64, 8, sulfur("wet"));

    REQUIRE(turned.has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "dry");
}

TEST_CASE("sulfur under still water over nothing in particular is merely wet") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("stone"), 2);

    REQUIRE(geysers::validState(loaded.state, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "wet");
}

TEST_CASE("lava under it makes it erupt without stopping") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("lava", {{"level", "0"}}), 2);

    REQUIRE(geysers::validState(loaded.state, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "continuous");

    // parity: flowing lava is not a source, so it drives nothing.
    Loaded flowing;
    flowing.geyser("dry", stateOf("lava", {{"level", "1"}}), 2);
    REQUIRE(geysers::validState(flowing.state, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(flowing.state, 8, 64, 8) == "wet");
}

TEST_CASE("a magma block under it makes it keep a clock, starting dormant") {
    Loaded loaded;
    loaded.geyser("dry", stateOf("magma_block"), 2);

    REQUIRE(geysers::validState(loaded.state, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "dormant");

    // One that was already erupting keeps erupting rather than resetting.
    Loaded erupting;
    erupting.geyser("erupting", stateOf("magma_block"), 2);
    CHECK_FALSE(geysers::validState(erupting.state, 0, 8, 64, 8, sulfur("erupting")).has_value());
    CHECK(stateAt(erupting.state, 8, 64, 8) == "erupting");
}

TEST_CASE("a state that is already right is left alone") {
    Loaded loaded;
    loaded.geyser("wet", stateOf("stone"), 2);

    CHECK_FALSE(geysers::validState(loaded.state, 0, 8, 64, 8, sulfur("wet")).has_value());
    CHECK(stateAt(loaded.state, 8, 64, 8) == "wet");
    // And a block that is not sulfur has no state to settle.
    CHECK_FALSE(geysers::validState(loaded.state, 0, 8, 64, 8, stateOf("stone")).has_value());
}

TEST_CASE("taking a clock on and losing it books nothing either way") {
    // parity: the wait is counted in the block entity, so no geyser ever puts a turn on the chunk.
    // Booking one here is what the jar's own `block_ticks` says is wrong.
    Loaded taking;
    taking.geyser("dry", stateOf("magma_block"), 2);
    REQUIRE(geysers::validState(taking.state, 0, 8, 64, 8, sulfur("dry")).has_value());
    CHECK(stateAt(taking.state, 8, 64, 8) == "dormant");
    CHECK_FALSE(taking.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));

    Loaded losing;
    losing.geyser("dormant", stateOf("stone"), 2);
    REQUIRE(geysers::validState(losing.state, 0, 8, 64, 8, sulfur("dormant")).has_value());
    CHECK(stateAt(losing.state, 8, 64, 8) == "wet");
    CHECK_FALSE(losing.state.hasScheduledBlockTick(8, 64, 8, geysers::block()));
}

TEST_CASE("the wait only moves on a twenty-tick boundary") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 2);

    // parity: the ticker is handed every tick and does nothing on nineteen out of every twenty.
    for (std::int64_t gameTime = 1; gameTime < 20; ++gameTime) {
        CHECK_FALSE(geysers::countdownTick(loaded.state, kSeed, gameTime, 8, 64, 8).has_value());
        CHECK(loaded.countdown() == -1);
    }
    CHECK_FALSE(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8).has_value());
    // parity: the first turn works the wait out and takes one off it in the same go.
    CHECK(loaded.countdown() > 0);
}

TEST_CASE("a deeper geyser waits longer to erupt") {
    const auto stepsOf = [](std::int32_t depth) {
        Loaded loaded;
        loaded.geyser("dormant", stateOf("magma_block"), depth);
        return loaded.stepsToFlip();
    };

    // parity: ten twenty-tick steps for every block of water over the first one.
    CHECK(stepsOf(3) - stepsOf(2) == 10);
    CHECK(stepsOf(4) - stepsOf(3) == 10);
}

TEST_CASE("two geysers side by side keep their own rhythm, and the same one every time") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 2);
    loaded.put(9, 63, 8, stateOf("magma_block"));
    loaded.put(9, 64, 8, sulfur("dormant"));
    loaded.withBlockEntity(9, 64, 8);
    loaded.put(9, 65, 8, stillWater());
    loaded.put(9, 66, 8, stillWater());

    // parity: `geyserPositional` forks the seed at the square, so the two columns count differently.
    static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8));
    static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 9, 64, 8));
    CHECK(loaded.countdown(8, 64, 8) != loaded.countdown(9, 64, 8));

    // And the same seed gives the same wait again.
    Loaded again;
    again.geyser("dormant", stateOf("magma_block"), 2);
    static_cast<void>(geysers::countdownTick(again.state, kSeed, 20, 8, 64, 8));
    CHECK(again.countdown() == loaded.countdown(8, 64, 8));
}

TEST_CASE("the wait flips dormant to erupting and back") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 2);

    const std::int32_t toErupt = loaded.stepsToFlip();
    REQUIRE(toErupt > 0);
    CHECK(stateAt(loaded.state, 8, 64, 8) == "erupting");
    CHECK(loaded.countdown() == 0);

    // parity: an eruption is short, `waterBlocks - 1 + [1, 2]` steps, and it puts the geyser back.
    const std::int32_t toRest = loaded.stepsToFlip();
    REQUIRE(toRest > 0);
    CHECK(toRest <= 3);
    CHECK(stateAt(loaded.state, 8, 64, 8) == "dormant");
}

TEST_CASE("only a geyser with a clock and a way out counts anything down") {
    // parity: `getTicker` gives a wet geyser no countdown at all, only the breath of gas.
    Loaded wet;
    wet.geyser("wet", stateOf("stone"), 2);
    CHECK_FALSE(geysers::countdownTick(wet.state, kSeed, 20, 8, 64, 8).has_value());
    CHECK(wet.countdown() == -1);

    Loaded plain;
    plain.put(8, 64, 8, stateOf("stone"));
    CHECK_FALSE(geysers::countdownTick(plain.state, kSeed, 20, 8, 64, 8).has_value());

    // A geyser under a lid has nowhere for the plume to go, so its wait does not move either.
    Loaded capped;
    capped.geyser("dormant", stateOf("magma_block"), 2);
    capped.put(8, 67, 8, stateOf("stone"));
    CHECK_FALSE(geysers::countdownTick(capped.state, kSeed, 20, 8, 64, 8).has_value());
    CHECK(capped.countdown() == -1);
    CHECK(stateAt(capped.state, 8, 64, 8) == "dormant");
}

TEST_CASE("more than four blocks of water over it is too deep to reach the top") {
    Loaded deep;
    deep.geyser("dormant", stateOf("magma_block"), 6);

    // parity: `ALLOWED_WATER_BLOCKS_ABOVE`, four and no more: with no way out there is nothing to
    // count down.
    static_cast<void>(geysers::countdownTick(deep.state, kSeed, 20, 8, 64, 8));
    CHECK(deep.countdown() == -1);

    Loaded shallow;
    shallow.geyser("dormant", stateOf("magma_block"), 4);
    static_cast<void>(geysers::countdownTick(shallow.state, kSeed, 20, 8, 64, 8));
    CHECK(shallow.countdown() > 0);
}

TEST_CASE("flowing water over it is not water it can rise through") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 1);
    loaded.put(8, 66, 8, flowingWater());

    // The flowing block is not a source, so the plume stops there and comes out of it.
    static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8));
    CHECK(loaded.countdown() > 0);
}

namespace {

/// The same draw the geyser makes, from the same seed at the same place.
[[nodiscard]] basalt::server::random::RandomSource geyserRandom(std::int64_t seed, std::int32_t x, std::int32_t y,
                                                                std::int32_t z) {
    basalt::server::random::RandomSource root = basalt::server::random::RandomSource::xoroshiro(seed ^ -904011478LL);
    return root.forkPositional().at(x, y, z);
}

} // namespace

TEST_CASE("the wait is ten steps per block of water plus a roll of fifteen to thirty") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 2);
    static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8));

    // parity: `SERVER_WAITING_COUNTDOWN_TICKER`'s own arithmetic, derived rather than remembered.
    // The turn that works the wait out also takes one off it.
    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    const std::int32_t steps = (10 * (2 - 1)) + source.nextIntBetweenInclusive(15, 30);
    CHECK(loaded.countdown() == steps - 1);
}

TEST_CASE("an erupting geyser waits a roll of one or two, after a draw it throws away") {
    Loaded loaded;
    loaded.geyser("erupting", stateOf("magma_block"), 3);
    static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8));

    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    // parity: the erupting branch draws one it never reads, which is what keeps the two apart.
    static_cast<void>(source.nextInt());
    const std::int32_t steps = 3 - 1 + source.nextIntBetweenInclusive(1, 2);
    CHECK(loaded.countdown() == steps - 1);
}

TEST_CASE("a geyser with no water over it has nothing to wait for and flips at once") {
    Loaded loaded;
    loaded.put(8, 63, 8, stateOf("magma_block"));
    loaded.put(8, 64, 8, sulfur("erupting"));
    loaded.withBlockEntity(8, 64, 8);

    // parity: no blocks of water makes the wait zero or one, and either way the same turn or the
    // next one puts the geyser back to sleep. Vanilla does not skip a wait of nothing.
    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    static_cast<void>(source.nextInt());
    const std::int32_t steps = 0 - 1 + source.nextIntBetweenInclusive(1, 2);
    const bool flipped = geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8).has_value();
    CHECK(flipped == (steps <= 0));
    CHECK(loaded.countdown() == 0);
}

TEST_CASE("exactly four blocks of water reach the top, and five do not") {
    for (std::int32_t depth = 1; depth <= 5; ++depth) {
        Loaded loaded;
        loaded.geyser("dormant", stateOf("magma_block"), depth);
        static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8));
        // parity: `ALLOWED_WATER_BLOCKS_ABOVE` is four, so the fifth block of water caps it and
        // there is nothing to count.
        CHECK((loaded.countdown() > 0) == (depth <= 4));
    }
}

TEST_CASE("only the block directly over it decides whether it is under water") {
    Loaded gap;
    gap.put(8, 63, 8, stateOf("magma_block"));
    gap.put(8, 64, 8, sulfur("wet"));
    // Water one higher, with air in between.
    gap.put(8, 66, 8, stillWater());

    REQUIRE(geysers::validState(gap.state, 0, 8, 64, 8, sulfur("wet")).has_value());
    CHECK(stateAt(gap.state, 8, 64, 8) == "dry");
}

TEST_CASE("a waterlogged block counts as the water over it") {
    Loaded logged;
    logged.put(8, 63, 8, stateOf("magma_block"));
    logged.put(8, 64, 8, sulfur("dry"));
    logged.put(8, 65, 8, stateOf("oak_slab", {{"type", "bottom"}, {"waterlogged", "true"}}));

    REQUIRE(geysers::validState(logged.state, 0, 8, 64, 8, sulfur("dry")).has_value());
    // parity: `isSourceIfFluid` asks the fluid, not the block, so a waterlogged slab is water.
    CHECK(stateAt(logged.state, 8, 64, 8) == "dormant");
    // The slab itself caps the plume, though, so there is nothing to count down.
    logged.withBlockEntity(8, 64, 8);
    static_cast<void>(geysers::countdownTick(logged.state, kSeed, 20, 8, 64, 8));
    CHECK(logged.countdown() == -1);
}

TEST_CASE("the plume comes out through anything it can pass, not only through air") {
    Loaded loaded;
    loaded.geyser("dormant", stateOf("magma_block"), 1);
    loaded.put(8, 66, 8, stateOf("torch"));

    // parity: `isGeyserPassableBlock`, which a torch is and a slab is not.
    static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8));
    CHECK(loaded.countdown() > 0);

    Loaded capped;
    capped.geyser("dormant", stateOf("magma_block"), 1);
    capped.put(8, 66, 8, stateOf("oak_slab", {{"type", "bottom"}, {"waterlogged", "false"}}));
    static_cast<void>(geysers::countdownTick(capped.state, kSeed, 20, 8, 64, 8));
    CHECK(capped.countdown() == -1);
}

TEST_CASE("a geyser outside the loaded world settles nothing") {
    Loaded loaded;

    CHECK_FALSE(geysers::validState(loaded.state, 0, 500, 64, 500, sulfur("wet")).has_value());
    CHECK_FALSE(geysers::countdownTick(loaded.state, kSeed, 20, 500, 64, 500).has_value());
}

TEST_CASE("the same arithmetic holds at every column, whatever the roll comes out as") {
    for (std::int32_t x = 2; x <= 13; ++x) {
        Loaded dormant;
        dormant.put(x, 63, 8, stateOf("magma_block"));
        dormant.put(x, 64, 8, sulfur("dormant"));
        dormant.put(x, 65, 8, stillWater());
        dormant.put(x, 66, 8, stillWater());
        dormant.withBlockEntity(x, 64, 8);
        static_cast<void>(geysers::countdownTick(dormant.state, kSeed, 20, x, 64, 8));
        basalt::server::random::RandomSource waiting = geyserRandom(kSeed, x, 64, 8);
        const std::int32_t waitSteps = (10 * (2 - 1)) + waiting.nextIntBetweenInclusive(15, 30);
        CHECK(dormant.countdown(x, 64, 8) == waitSteps - 1);

        Loaded erupting;
        erupting.put(x, 63, 8, stateOf("magma_block"));
        erupting.put(x, 64, 8, sulfur("erupting"));
        erupting.put(x, 65, 8, stillWater());
        erupting.put(x, 66, 8, stillWater());
        erupting.withBlockEntity(x, 64, 8);
        static_cast<void>(geysers::countdownTick(erupting.state, kSeed, 20, x, 64, 8));
        basalt::server::random::RandomSource going = geyserRandom(kSeed, x, 64, 8);
        static_cast<void>(going.nextInt());
        const std::int32_t goingSteps = 2 - 1 + going.nextIntBetweenInclusive(1, 2);
        CHECK(erupting.countdown(x, 64, 8) == goingSteps - 1);
    }
}

TEST_CASE("a wait of one step is spent by the same turn that works it out") {
    // With no water over it the wait comes out as the roll less one, so it is one step or none, and
    // both are gone by the end of that turn: the decrement and the flip are the same three lines.
    bool sawOne = false;
    bool sawNone = false;
    for (std::int32_t x = 2; x <= 13 && !(sawOne && sawNone); ++x) {
        basalt::server::random::RandomSource source = geyserRandom(kSeed, x, 64, 8);
        static_cast<void>(source.nextInt());
        const std::int32_t roll = source.nextIntBetweenInclusive(1, 2);

        Loaded loaded;
        loaded.put(x, 63, 8, stateOf("magma_block"));
        loaded.put(x, 64, 8, sulfur("erupting"));
        loaded.withBlockEntity(x, 64, 8);
        CHECK(geysers::countdownTick(loaded.state, kSeed, 20, x, 64, 8).has_value());
        CHECK(stateAt(loaded.state, x, 64, 8) == "dormant");
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
    loaded.withBlockEntity(8, 64, 8);
    static_cast<void>(geysers::countdownTick(loaded.state, kSeed, 20, 8, 64, 8));

    // Two blocks of water, not one: the lichen counts as water and is passed rather than stopped at.
    basalt::server::random::RandomSource source = geyserRandom(kSeed, 8, 64, 8);
    const std::int32_t steps = (10 * (2 - 1)) + source.nextIntBetweenInclusive(15, 30);
    CHECK(loaded.countdown() == steps - 1);
}
