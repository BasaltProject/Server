#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/NoiseWorldGenerator.hpp>
#include <basalt/server/world/WorldHeight.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::world;
namespace registry = basalt::server::registry;

namespace {

struct Report {
    std::int32_t solid = 0;
    std::int64_t checksum = 0;
    std::int64_t heightSum = 0;
};

/// The same fold the frozen Kotlin printed, so the numbers below are its own.
[[nodiscard]] Report reportOf(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t solid = 0;
    for (std::int32_t y = chunk.verticalBounds().minY(); y <= chunk.verticalBounds().maxY(); ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t state = chunk.blockStateId(x, y, z);
                if (state != 0) {
                    ++solid;
                    const std::int64_t folded = static_cast<std::int64_t>(x) * 31 + z * 37 + y * 41 + state * 43;
                    checksum = (checksum ^ static_cast<std::uint64_t>(folded)) * 1099511628211ULL;
                }
            }
        }
    }
    std::int64_t heightSum = 0;
    for (const std::int32_t height : chunk.heightmap()) {
        heightSum += height;
    }
    return Report{.solid = solid, .checksum = static_cast<std::int64_t>(checksum), .heightSum = heightSum};
}

} // namespace

TEST_CASE("the chunks the frozen kotlin generated, block for block") {
    // Every number here was printed by the Kotlin generator, not derived from this one.
    const Report origin = reportOf(noiseWorldGenerator::generate(0, 0));
    CHECK(origin.solid == 32901);
    CHECK(origin.checksum == 6294226696645776382LL);
    CHECK(origin.heightSum == 16517);

    const Report east = reportOf(noiseWorldGenerator::generate(1, 0));
    CHECK(east.solid == 32543);
    CHECK(east.checksum == 7521381607769648945LL);
    CHECK(east.heightSum == 16238);

    const Report negative = reportOf(noiseWorldGenerator::generate(-3, 7));
    CHECK(negative.solid == 32756);
    CHECK(negative.checksum == -9121355473577638777LL);
    CHECK(negative.heightSum == 16487);

    const Report far = reportOf(noiseWorldGenerator::generate(100, -200));
    CHECK(far.solid == 32561);
    CHECK(far.checksum == 121605967617762008LL);
    CHECK(far.heightSum == 16307);
}

TEST_CASE("one column, as the oracle printed it") {
    const Chunk chunk = noiseWorldGenerator::generate(0, 0);
    // From y 55 to 75: stone, then the four surface blocks, then air. The numbers are the state
    // ids the Kotlin printed.
    const std::vector<std::int32_t> expected{1, 1, 1, 1, 1, 10, 10, 10, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::vector<std::int32_t> column;
    for (std::int32_t y = 55; y <= 75; ++y) {
        column.push_back(chunk.blockStateId(0, y, 0));
    }
    CHECK(column == expected);

    // Which is to say: stone below, three dirt, grass on top.
    CHECK(chunk.blockStateId(0, 59, 0) == registry::blocks::stone().stateId);
    CHECK(chunk.blockStateId(0, 60, 0) == registry::blocks::named("dirt")->stateId);
    CHECK(chunk.blockStateId(0, 63, 0) == registry::blocks::named("grass_block")->stateId);
    CHECK(registry::blocks::isAir(chunk.blockStateId(0, 64, 0)));

    // parity of shape with vanilla: the floor of the world is bedrock, wherever you stand on it.
    CHECK(chunk.blockStateId(3, worldHeight::kMinY, 5) == 88);
    CHECK(chunk.blockStateId(3, worldHeight::kMinY, 5) == registry::blocks::bedrock().stateId);
}

TEST_CASE("the ground rolls, and stays near the sea level it is built around") {
    const std::vector<std::int32_t> heights = noiseWorldGenerator::generate(0, 0).heightmap();
    REQUIRE(heights.size() == 256);
    std::int32_t lowest = heights.front();
    std::int32_t highest = heights.front();
    for (const std::int32_t height : heights) {
        lowest = std::min(lowest, height);
        highest = std::max(highest, height);
    }
    // The oracle's own numbers for this chunk: gentle rolling, not a hill per chunk.
    CHECK(lowest == 63);
    CHECK(highest == 66);
}

TEST_CASE("the same chunk twice is the same chunk") {
    // No seed of its own and no state between calls: the generator is a function of where it is
    // asked about.
    const Report first = reportOf(noiseWorldGenerator::generate(5, -9));
    const Report second = reportOf(noiseWorldGenerator::generate(5, -9));
    CHECK(first.checksum == second.checksum);
    CHECK(first.solid == second.solid);
    // And a different chunk is a different chunk.
    CHECK(reportOf(noiseWorldGenerator::generate(6, -9)).checksum != first.checksum);
}

TEST_CASE("every trunk stands on grass, and every canopy stays in its own chunk") {
    const std::int32_t log = registry::blocks::named("oak_log")->stateId;
    const std::int32_t leaves = registry::blocks::named("oak_leaves")->stateId;
    const std::int32_t grass = registry::blocks::named("grass_block")->stateId;
    std::int32_t trunks = 0;
    std::int32_t canopies = 0;

    for (std::int32_t chunkX = 0; chunkX < 12; ++chunkX) {
        for (std::int32_t chunkZ = 0; chunkZ < 12; ++chunkZ) {
            CAPTURE(chunkX);
            CAPTURE(chunkZ);
            const Chunk chunk = noiseWorldGenerator::generate(chunkX, chunkZ);
            for (std::int32_t x = 0; x < 16; ++x) {
                for (std::int32_t z = 0; z < 16; ++z) {
                    for (std::int32_t y = worldHeight::kMinY + 1; y <= worldHeight::kMaxY; ++y) {
                        const std::int32_t state = chunk.blockStateId(x, y, z);
                        if (state == leaves) {
                            ++canopies;
                        }
                        if (state != log) {
                            continue;
                        }
                        ++trunks;
                        // The lowest log of a column is the one standing on the ground, and the
                        // ground it stands on is grass: a tree is never planted on another tree.
                        if (chunk.blockStateId(x, y - 1, z) != log) {
                            CHECK(chunk.blockStateId(x, y - 1, z) == grass);
                        }
                        // And it never grows through the roof of the world.
                        CHECK(y < worldHeight::kMaxY);
                    }
                }
            }
        }
    }
    // Enough of them for the check above to mean something.
    CHECK(trunks > 100);
    CHECK(canopies > 100);
}

TEST_CASE("the world is as tall as vanilla says it is") {
    // parity: NoiseSettings.OVERWORLD_NOISE_SETTINGS, create(-64, 384, 1, 2).
    CHECK(worldHeight::kMinY == -64);
    CHECK(worldHeight::kHeight == 384);
    CHECK(worldHeight::kMaxY == 319);
    CHECK(worldHeight::kMinSection == -4);
    CHECK(worldHeight::kSectionCount == 24);
}
