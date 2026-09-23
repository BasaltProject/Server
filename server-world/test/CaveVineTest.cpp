#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveVineColumnFeature.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
namespace registry = basalt::server::registry;

namespace {

/// A ceiling with air below it, and a floor part way down: the column grows downward until it hits
/// something, so an unbounded shaft would never exercise the truncation and a solid mass would never
/// let it start.
[[nodiscard]] Chunk shaftChunk(std::int32_t floorY) {
    Chunk chunk{0, 0};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -60; y < floorY; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = 80; y < 90; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t vines = 0;
    std::set<std::int32_t> distinct;
    for (std::int32_t y = -60; y <= 90; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && (block->path() == "cave_vines" || block->path() == "cave_vines_plant")) {
                    ++vines;
                    distinct.insert(id);
                }
            }
        }
    }
    return {checksum.value(), vines, distinct.size()};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::string, std::int64_t> run(bool inMoss,
                                                                                                 std::int32_t floorY) {
    testsupport::Checksum checksum;
    std::int32_t vines = 0;
    std::size_t distinct = 0;
    std::int32_t placed = 0;
    std::string heights;
    for (std::int32_t index = 0; index < 40; ++index) {
        Chunk chunk = shaftChunk(floorY);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(383838ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        if (caveVineColumnFeature::place(chunk, random, DecorationPos{8, 79, 8}, inMoss)) {
            ++placed;
        }
        const auto [chunkChecksum, chunkVines, chunkDistinct] = fold(chunk);
        checksum.feed(chunkChecksum);
        vines += chunkVines;
        distinct += chunkDistinct;
        heights.append(std::to_string(chunkVines)).push_back(',');
    }
    return {placed, vines, distinct, heights, checksum.value()};
}

} // namespace

TEST_CASE("the cave vine columns match the frozen kotlin") {
    SUBCASE("cave") {
        const auto [placed, vines, distinct, columnHeights, checksum] = run(false, 40);
        CHECK(placed == 40);
        CHECK(vines == 187);
        // Distinct states, not just a count: the tip carries an age and a berries flag.
        CHECK(distinct == 87);
        CHECK(columnHeights == "5,3,7,4,12,1,1,2,4,1,1,6,7,3,12,11,4,5,2,1,3,7,5,1,11,2,6,6,6,3,3,6,2,16,2,5,2,4,4,1,");
        CHECK(checksum == -378055848100785941LL);
    }

    SUBCASE("moss") {
        const auto [placed, vines, distinct, columnHeights, checksum] = run(true, 40);
        CHECK(placed == 40);
        CHECK(vines == 112);
        // Distinct states, not just a count: the tip carries an age and a berries flag.
        CHECK(distinct == 72);
        CHECK(columnHeights == "3,2,2,4,3,4,3,4,1,4,1,7,3,1,2,2,4,1,1,1,2,1,2,1,4,1,4,7,4,1,2,7,1,3,3,6,2,5,1,2,");
        CHECK(checksum == -2186008846919537979LL);
    }

    SUBCASE("caveShort") {
        // A high floor truncates the column: the body is removed first and the tip kept, so
        // the heights cap out well below the unbounded case.
        const auto [placed, vines, distinct, columnHeights, checksum] = run(false, 76);
        CHECK(placed == 40);
        CHECK(vines == 100);
        // Distinct states, not just a count: the tip carries an age and a berries flag.
        CHECK(distinct == 79);
        CHECK(columnHeights == "3,3,3,3,3,1,1,2,3,1,1,3,3,3,3,3,3,3,2,1,3,3,3,1,3,2,3,3,3,3,3,3,2,3,2,3,2,3,3,1,");
        CHECK(checksum == 3000659158143803852LL);
    }
}
