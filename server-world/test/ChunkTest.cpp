#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/ChunkHeight.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/server/world/PalettedContainer.hpp>

#include <doctest.h>

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
