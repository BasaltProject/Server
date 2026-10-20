#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>

using basalt::wire::common::Identifier;
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkTypes.hpp>
#include <basalt/server/world/PalettedContainer.hpp>

#include <doctest.h>

#include <cstdint>
#include <thread>
#include <vector>

using namespace basalt::server::world;

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
    CHECK(container.get(0) == 1);
    for (std::int32_t value = 2; value <= 16; ++value) {
        CAPTURE(value);
        CHECK(container.get(static_cast<std::size_t>(value)) == value);
    }
    CHECK(container.get(1) == 0);
    CHECK(container.get(3000) == 0);
}

TEST_CASE("an entry that straddles two words reads back whole") {
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

TEST_CASE("a reader never sees a palette index its palette does not have") {
    PalettedContainer container{4096, 0};
    container.share();
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

TEST_CASE("a block entity is named after its type, however the caller names it") {
    const ChunkBlockEntity fromBlock{1, 64, 2, Identifier::vanilla("copper_chest"), {}};
    CHECK(fromBlock.type() == Identifier::vanilla("chest"));

    const ChunkBlockEntity fromType{1, 64, 2, Identifier::vanilla("chest"), {}};
    CHECK(fromType.type() == Identifier::vanilla("chest"));

    const ChunkBlockEntity plain{1, 64, 2, Identifier::vanilla("stone"), {}};
    CHECK(plain.type() == Identifier::vanilla("stone"));
}

TEST_CASE("ticks count down and come due once") {
    Chunk chunk{0, 0};
    chunk.scheduleBlockTick({Identifier::vanilla("water"), 1, 64, 2, 2, 0});
    chunk.scheduleBlockTick({Identifier::vanilla("water"), 1, 64, 2, 5, 0});
    CHECK(chunk.scheduledTickCount() == 1);

    // parity: ChunkAccess absolute triggerTick, a turn cannot shorten what it booked itself.
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
    Chunk chunk{0, 0};
    chunk.openTurn();
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

    CHECK_FALSE(chunk.setBlockStateId(0, 64, 0, blocks::stone().stateId));
    CHECK_FALSE(chunk.isDirty());

    // parity: ChunkAccess.incrementInhabitedTime never dirties the chunk.
    chunk.restoreInhabitedTime(500);
    CHECK_FALSE(chunk.isDirty());
    CHECK(chunk.inhabitedTime() == 500);
    chunk.increaseInhabitedTime(1);
    CHECK_FALSE(chunk.isDirty());
    CHECK(chunk.inhabitedTime() == 501);
}
