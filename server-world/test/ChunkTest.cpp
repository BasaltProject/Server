#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/world/PalettedContainer.hpp>

#include <doctest.h>

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
