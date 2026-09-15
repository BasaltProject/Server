#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>

using basalt::wire::common::Identifier;
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/server/world/PalettedContainer.hpp>

#include <doctest.h>

#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace basalt::server::world;

TEST_CASE("a container of one value costs no data array at all") {
    // All-air is the common case by far, and it is what makes keeping chunks in memory affordable.
    PalettedContainer container{4096, 7};
    CHECK(container.bitsPerValue() == 0);
    CHECK(container.distinctValueCount() == 1);
    CHECK(container.get(0) == 7);
    CHECK(container.get(4095) == 7);
    // Writing the value it already holds changes nothing, and says so.
    CHECK_FALSE(container.set(10, 7));
    CHECK(container.bitsPerValue() == 0);
}

TEST_CASE("the entries widen only when the palette outgrows them") {
    PalettedContainer container{4096, 0};
    CHECK(container.set(0, 1));
    // parity: an indirect palette never goes below four bits.
    CHECK(container.bitsPerValue() == 4);

    for (std::int32_t value = 2; value <= 16; ++value) {
        CHECK(container.set(static_cast<std::size_t>(value), value));
    }
    CHECK(container.distinctValueCount() == 17);
    CHECK(container.bitsPerValue() == 5);
    // Widening moves no values: every one of them still reads back, the first write included.
    CHECK(container.get(0) == 1);
    for (std::int32_t value = 2; value <= 16; ++value) {
        CAPTURE(value);
        CHECK(container.get(static_cast<std::size_t>(value)) == value);
    }
    // Index 1 was never written, and an untouched entry is still the value it started as.
    CHECK(container.get(1) == 0);
    CHECK(container.get(3000) == 0);
}

TEST_CASE("an entry that straddles two words reads back whole") {
    // Five bits into sixty four leaves entries crossing the boundary, which is where a packing bug
    // shows up and nowhere else.
    PalettedContainer container{4096, 0};
    for (std::int32_t value = 1; value <= 20; ++value) {
        container.set(static_cast<std::size_t>(value), value);
    }
    REQUIRE(container.bitsPerValue() == 5);
    for (std::size_t index = 0; index < 200; ++index) {
        container.set(index, static_cast<std::int32_t>(index % 20) + 1);
    }
    for (std::size_t index = 0; index < 200; ++index) {
        CAPTURE(index);
        REQUIRE(container.get(index) == static_cast<std::int32_t>(index % 20) + 1);
    }
}

TEST_CASE("the palette alone answers what a section may hold") {
    // This is what makes a whole-world search affordable: a section that cannot hold what is being
    // looked for is skipped without reading a single packed word.
    PalettedContainer container{4096, 0};
    container.set(5, 42);
    CHECK(container.anyValue([](std::int32_t value) { return value == 42; }));
    CHECK_FALSE(container.anyValue([](std::int32_t value) { return value == 43; }));
}

TEST_CASE("expanding takes one snapshot and gives every value") {
    PalettedContainer container{64, 3};
    container.set(0, 9);
    container.set(63, 11);
    const std::vector<std::int32_t> values = container.toVector();
    REQUIRE(values.size() == 64);
    CHECK(values[0] == 9);
    CHECK(values[1] == 3);
    CHECK(values[63] == 11);

    // And back the other way, re-deriving the palette.
    PalettedContainer other{64, 0};
    other.setAll(values);
    for (std::size_t index = 0; index < values.size(); ++index) {
        CAPTURE(index);
        REQUIRE(other.get(index) == values[index]);
    }
}

TEST_CASE("a reader never sees a palette index its palette does not have") {
    // The whole point of swapping the storage as one: a connection thread reads a chunk to send it
    // while the world is still being written to. Read separately, a widening write would hand the
    // reader the new width with the old array.
    PalettedContainer container{4096, 0};
    std::atomic<bool> stop{false};
    std::atomic<bool> sawGarbage{false};

    std::thread reader{[&] {
        std::vector<std::int32_t> scratch(4096);
        while (!stop.load()) {
            container.copyInto(scratch);
            for (const std::int32_t value : scratch) {
                if (value < 0 || value > 400) {
                    sawGarbage.store(true);
                }
            }
        }
    }};

    for (std::int32_t value = 1; value <= 400; ++value) {
        container.set(static_cast<std::size_t>(value) % 4096, value);
    }
    stop.store(true);
    reader.join();
    CHECK_FALSE(sawGarbage.load());
}

TEST_CASE("a section keeps its counts on write instead of recounting them") {
    // Recounting scanned all 4096 entries every time, which is once per section per chunk send.
    using namespace basalt::server::registry;
    ChunkSection section;
    CHECK(section.nonAirBlockCount() == 0);
    CHECK(section.nonEmptyFluidCount() == 0);

    CHECK(section.setBlockStateId(1, 2, 3, blocks::stone().stateId));
    CHECK(section.nonAirBlockCount() == 1);
    CHECK(section.nonEmptyFluidCount() == 0);

    CHECK(section.setBlockStateId(4, 5, 6, blocks::water().stateId));
    CHECK(section.nonAirBlockCount() == 2);
    CHECK(section.nonEmptyFluidCount() == 1);

    // Putting air back takes the block out of both counts it was in.
    CHECK(section.setBlockStateId(4, 5, 6, blocks::air().stateId));
    CHECK(section.nonAirBlockCount() == 1);
    CHECK(section.nonEmptyFluidCount() == 0);

    // Writing what is already there changes nothing, counts included.
    CHECK_FALSE(section.setBlockStateId(1, 2, 3, blocks::stone().stateId));
    CHECK(section.nonAirBlockCount() == 1);
}

TEST_CASE("filling a section wholesale counts what it was given") {
    using namespace basalt::server::registry;
    ChunkSection section;
    std::vector<std::int32_t> ids(ChunkSection::kBlocks, blocks::air().stateId);
    for (std::size_t i = 0; i < 100; ++i) {
        ids[i] = blocks::stone().stateId;
    }
    for (std::size_t i = 100; i < 150; ++i) {
        ids[i] = blocks::water().stateId;
    }
    section.setBlockStateIds(ids);
    CHECK(section.nonAirBlockCount() == 150);
    CHECK(section.nonEmptyFluidCount() == 50);
    CHECK(section.blockStateId(0, 0, 0) == blocks::stone().stateId);
    CHECK(section.blockStateIds() == ids);
}

TEST_CASE("a section refuses a coordinate outside it rather than wrapping") {
    ChunkSection section;
    CHECK_THROWS_AS(static_cast<void>(section.blockStateId(16, 0, 0)), std::out_of_range);
    CHECK_THROWS_AS(static_cast<void>(section.blockStateId(0, -1, 0)), std::out_of_range);
    CHECK_THROWS_AS(static_cast<void>(section.biomeId(4, 0, 0)), std::out_of_range);
}

TEST_CASE("biomes sit at quart resolution and survive a round trip") {
    ChunkSection section;
    section.setBiomeId(1, 2, 3, 42);
    CHECK(section.biomeId(1, 2, 3) == 42);
    CHECK(section.biomeId(0, 0, 0) == 0);

    std::vector<std::int32_t> ids(ChunkSection::kBiomes, 7);
    ids[5] = 9;
    section.setBiomeIds(ids);
    CHECK(section.biomeIds() == ids);
}

TEST_CASE("the overworld is twenty four sections from sixty four below zero") {
    // parity: NoiseSettings.OVERWORLD_NOISE_SETTINGS, create(-64, 384, 1, 2).
    CHECK(kOverworldHeight.minY() == -64);
    CHECK(kOverworldHeight.maxY() == 319);
    CHECK(kOverworldHeight.sectionCount() == 24);
    CHECK(kOverworldHeight.minSection() == -4);
    CHECK(kOverworldHeight.maxSection() == 19);
    CHECK(kNetherHeight.sectionCount() == 16);
    // A height the chunk NBT could not store its sections in is not a valid one.
    CHECK_FALSE(ChunkHeight{-64, 100}.isValid());
    CHECK_FALSE(ChunkHeight{-70, 384}.isValid());
}

TEST_CASE("a block entity is named after its type, however the caller names it") {
    // Callers hand in a block, since a block is what they have. Getting this wrong left a copper
    // chest without a protocol id, so it never reached the client and rendered as nothing.
    using basalt::wire::common::Identifier;
    const ChunkBlockEntity fromBlock{1, 64, 2, Identifier::vanilla("copper_chest"), {}};
    CHECK(fromBlock.type() == Identifier::vanilla("chest"));

    const ChunkBlockEntity fromType{1, 64, 2, Identifier::vanilla("chest"), {}};
    CHECK(fromType.type() == Identifier::vanilla("chest"));

    // A block that owns no block entity keeps the name it was given.
    const ChunkBlockEntity plain{1, 64, 2, Identifier::vanilla("stone"), {}};
    CHECK(plain.type() == Identifier::vanilla("stone"));
}

TEST_CASE("a block entity outside its chunk is refused rather than stored") {
    using basalt::wire::common::Identifier;
    CHECK_THROWS_AS((ChunkBlockEntity{16, 64, 0, Identifier::vanilla("chest"), {}}), std::out_of_range);
    CHECK_THROWS_AS((ChunkBlockEntity{0, 400, 0, Identifier::vanilla("chest"), {}}), std::out_of_range);
    CHECK(isInsideChunk(0, -64, 0, kOverworldHeight));
    CHECK(isInsideChunk(15, 319, 15, kOverworldHeight));
    CHECK_FALSE(isInsideChunk(0, 320, 0, kOverworldHeight));
    // The nether is shorter, and what fits the overworld does not always fit it.
    CHECK_FALSE(isInsideChunk(0, 300, 0, kNetherHeight));
}

TEST_CASE("the heightmap is kept as blocks are written, not rescanned") {
    // Rescanning cost 2.6 million block reads per generated chunk, decoration alone asking for
    // 10000 columns.
    using namespace basalt::server::registry;
    Chunk chunk{0, 0};
    CHECK(chunk.heightAt(0, 0) == chunk.verticalBounds().minY());

    chunk.setBlockStateId(0, 64, 0, blocks::stone().stateId);
    CHECK(chunk.heightAt(0, 0) == 65);
    CHECK(chunk.heightAt(0, 0, HeightmapType::WorldSurface) == 65);
    CHECK(chunk.heightAt(1, 0) == chunk.verticalBounds().minY());

    // A block above raises it.
    chunk.setBlockStateId(0, 70, 0, blocks::stone().stateId);
    CHECK(chunk.heightAt(0, 0) == 71);

    // Taking the topmost one away is the only case that has to look downwards.
    chunk.setBlockStateId(0, 70, 0, blocks::air().stateId);
    CHECK(chunk.heightAt(0, 0) == 65);
}

TEST_CASE("a rebuilt heightmap says what the maintained one does") {
    using namespace basalt::server::registry;
    Chunk chunk{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, 60 + ((x + z) % 5), z, blocks::stone().stateId);
        }
    }
    const std::vector<std::int32_t> maintained = chunk.heightmap();
    // Writing through the section goes behind the chunk's back, which is what invalidation is for.
    chunk.invalidateHeightmaps();
    const std::vector<std::int32_t> rebuilt = chunk.heightmap();
    CHECK(maintained == rebuilt);
}

TEST_CASE("water counts for motion blocking but not for the ocean floor") {
    using namespace basalt::server::registry;
    Chunk chunk{0, 0};
    chunk.setBlockStateId(3, 62, 4, blocks::water().stateId);
    CHECK(chunk.heightAt(3, 4, HeightmapType::MotionBlocking) == 63);
    CHECK(chunk.heightAt(3, 4, HeightmapType::WorldSurface) == 63);
    // parity: OCEAN_FLOOR is what stops motion, and water does not.
    CHECK(chunk.heightAt(3, 4, HeightmapType::OceanFloor) == chunk.verticalBounds().minY());
}

TEST_CASE("leaves are left out of the no-leaves heightmap") {
    using namespace basalt::server::registry;
    Chunk chunk{0, 0};
    const std::optional<std::int32_t> leaves = blocks::stateIdOf(
        Identifier::vanilla("oak_leaves"), {{"distance", "1"}, {"persistent", "false"}, {"waterlogged", "false"}});
    REQUIRE(leaves.has_value());
    chunk.setBlockStateId(5, 80, 5, *leaves);
    CHECK(chunk.heightAt(5, 5, HeightmapType::MotionBlocking) == 81);
    CHECK(chunk.heightAt(5, 5, HeightmapType::MotionBlockingNoLeaves) == chunk.verticalBounds().minY());
}

TEST_CASE("a block entity survives a state change but not a block change") {
    // parity: LevelChunk.setBlockState drops it only when the *block* changes. A furnace lighting up
    // keeps what is inside it.
    using namespace basalt::server::registry;
    Chunk chunk{0, 0};
    chunk.setBlockStateId(1, 64, 2, blocks::stone().stateId);
    chunk.setBlockEntity(ChunkBlockEntity{1, 64, 2, Identifier::vanilla("chest"), {}});
    REQUIRE(chunk.blockEntityAt(1, 64, 2) != nullptr);

    const std::optional<std::int32_t> lit =
        blocks::stateIdOf(Identifier::vanilla("furnace"), {{"facing", "north"}, {"lit", "true"}});
    const std::optional<std::int32_t> unlit =
        blocks::stateIdOf(Identifier::vanilla("furnace"), {{"facing", "north"}, {"lit", "false"}});
    REQUIRE(lit.has_value());
    REQUIRE(unlit.has_value());
    chunk.setBlockStateId(3, 64, 3, *unlit);
    chunk.setBlockEntity(ChunkBlockEntity{3, 64, 3, Identifier::vanilla("furnace"), {}});
    chunk.setBlockStateId(3, 64, 3, *lit);
    CHECK(chunk.blockEntityAt(3, 64, 3) != nullptr);

    // Becoming another block does drop it.
    chunk.setBlockStateId(3, 64, 3, blocks::stone().stateId);
    CHECK(chunk.blockEntityAt(3, 64, 3) == nullptr);
}

TEST_CASE("ticks count down and come due once") {
    using basalt::wire::common::Identifier;
    Chunk chunk{0, 0};
    chunk.scheduleBlockTick({Identifier::vanilla("water"), 1, 64, 2, 2, 0});
    // The same position and type twice is one tick, not two.
    chunk.scheduleBlockTick({Identifier::vanilla("water"), 1, 64, 2, 5, 0});
    CHECK(chunk.scheduledTickCount() == 1);

    // parity: the counting down happens once, when the turn opens, and the pass only collects what
    // is already due. That is how the jar's absolute `triggerTick` behaves: a turn cannot shorten
    // what it booked itself.
    std::vector<ChunkScheduledTick> due;
    chunk.openTurn();
    chunk.advanceBlockTicks(due);
    CHECK(due.empty());
    CHECK(chunk.scheduledBlockTicks().front().delay == 1);

    chunk.openTurn();
    chunk.advanceBlockTicks(due);
    CHECK(due.size() == 1);
    CHECK(chunk.scheduledTickCount() == 0);
}

TEST_CASE("a turn does not count down what that same turn booked") {
    using basalt::wire::common::Identifier;
    Chunk chunk{0, 0};
    chunk.openTurn();
    // Booked after this turn opened: it keeps its two, and the next turn is the first to touch it.
    chunk.scheduleBlockTick({Identifier::vanilla("water"), 1, 64, 2, 2, 0});
    CHECK(chunk.scheduledBlockTicks().front().delay == 2);

    std::vector<ChunkScheduledTick> due;
    chunk.advanceBlockTicks(due);
    CHECK(due.empty());
    CHECK(chunk.scheduledBlockTicks().front().delay == 2);

    chunk.openTurn();
    chunk.advanceBlockTicks(due);
    CHECK(due.empty());
    CHECK(chunk.scheduledBlockTicks().front().delay == 1);
}

TEST_CASE("a chunk is dirty only when it holds something the region file does not") {
    using namespace basalt::server::registry;
    Chunk chunk{0, 0};
    CHECK_FALSE(chunk.isDirty());
    chunk.setBlockStateId(0, 64, 0, blocks::stone().stateId);
    CHECK(chunk.isDirty());
    chunk.markClean();

    // Writing the same state again changes nothing, so it does not dirty the chunk either.
    CHECK_FALSE(chunk.setBlockStateId(0, 64, 0, blocks::stone().stateId));
    CHECK_FALSE(chunk.isDirty());

    // parity: `ChunkAccess.incrementInhabitedTime` adds and nothing else, so neither loading the
    // value nor advancing it dirties the chunk. Advancing it happens every tick for every simulated
    // chunk, and dirtying there rewrote the whole region on every save.
    chunk.restoreInhabitedTime(500);
    CHECK_FALSE(chunk.isDirty());
    CHECK(chunk.inhabitedTime() == 500);
    chunk.increaseInhabitedTime(1);
    CHECK_FALSE(chunk.isDirty());
    CHECK(chunk.inhabitedTime() == 501);
}

TEST_CASE("the worldgen heights are frozen where the carvers left them") {
    // An ore's probe must see bare terrain, not what earlier features piled on top.
    using namespace basalt::server::registry;
    Chunk chunk{0, 0};
    chunk.setBlockStateId(2, 60, 2, blocks::stone().stateId);
    chunk.freezeWorldgenHeights();
    CHECK(chunk.worldgenHeightAt(2, 2, HeightmapType::OceanFloor) == 61);

    // A feature piling on afterwards moves the live map and leaves the frozen one alone.
    chunk.setBlockStateId(2, 90, 2, blocks::stone().stateId);
    CHECK(chunk.heightAt(2, 2, HeightmapType::OceanFloor) == 91);
    CHECK(chunk.worldgenHeightAt(2, 2, HeightmapType::OceanFloor) == 61);
}
