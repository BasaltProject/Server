#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/LeafDistances.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// A canopy over two trunks with a detached clump beside it: the detached leaves keep the decay
/// distance because nothing sourced them, and the two trunks meet in the middle of the canopy.
[[nodiscard]] Chunk canopyChunk(std::int32_t chunkX, std::int32_t chunkZ, std::string_view kind) {
    Chunk chunk{chunkX, chunkZ};
    const std::int32_t log = stateIdOf(std::string{kind} + "_log");
    const std::int32_t leaves = stateIdOf(std::string{kind} + "_leaves");
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, 60, z, stateIdOf("dirt"));
        }
    }
    for (const auto& [trunkX, trunkZ] : {std::pair{4, 4}, std::pair{11, 10}}) {
        for (std::int32_t y = 61; y <= 68; ++y) {
            chunk.setBlockStateId(trunkX, y, trunkZ, log);
        }
    }
    for (std::int32_t x = 1; x < 15; ++x) {
        for (std::int32_t z = 1; z < 15; ++z) {
            for (std::int32_t y = 66; y <= 70; ++y) {
                if (chunk.blockStateId(x, y, z) == log) {
                    continue;
                }
                chunk.setBlockStateId(x, y, z, leaves);
            }
        }
    }
    // A short tree whose whole trunk sits in the section below its leaves: without the extra
    // section either way, the walk would start above the only thing sourcing this canopy.
    for (std::int32_t y = 61; y <= 63; ++y) {
        chunk.setBlockStateId(14, y, 2, log);
    }
    for (std::int32_t y = 64; y <= 66; ++y) {
        for (std::int32_t x = 13; x < 16; ++x) {
            for (std::int32_t z = 1; z < 4; ++z) {
                if (chunk.blockStateId(x, y, z) == log) {
                    continue;
                }
                chunk.setBlockStateId(x, y, z, leaves);
            }
        }
    }
    // A clump with no trunk under it, far enough that the walk never reaches it.
    for (std::int32_t y = 74; y <= 76; ++y) {
        for (std::int32_t x = 0; x < 3; ++x) {
            for (std::int32_t z = 13; z < 16; ++z) {
                chunk.setBlockStateId(x, y, z, leaves);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t leaves = 0;
    for (std::int32_t y = 55; y <= 80; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && block->path().ends_with("_leaves")) {
                    ++leaves;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), leaves};
}

} // namespace

TEST_CASE("the leaf distance pass stamps what the frozen kotlin stamps") {
    SUBCASE("oak") {
        Chunk chunk = canopyChunk(0, 0, "oak");
        CHECK(leafDistances::assign(chunk) == 646);
        auto [checksum, leaves] = fold(chunk);
        CHECK(leaves == 1022);
        CHECK(checksum == 514877107340895582LL);
        // A second pass has nothing left to correct: the first one is a fixed point.
        CHECK(leafDistances::assign(chunk) == 0);
        CHECK(fold(chunk).first == 514877107340895582LL);
    }

    SUBCASE("spruce") {
        Chunk chunk = canopyChunk(0, 0, "spruce");
        CHECK(leafDistances::assign(chunk) == 646);
        auto [checksum, leaves] = fold(chunk);
        CHECK(leaves == 1022);
        CHECK(checksum == 3500298826586513735LL);
        // A second pass has nothing left to correct: the first one is a fixed point.
        CHECK(leafDistances::assign(chunk) == 0);
        CHECK(fold(chunk).first == 3500298826586513735LL);
    }

    SUBCASE("mangrove") {
        Chunk chunk = canopyChunk(0, 0, "mangrove");
        CHECK(leafDistances::assign(chunk) == 646);
        auto [checksum, leaves] = fold(chunk);
        CHECK(leaves == 1022);
        CHECK(checksum == -1372352672412502890LL);
        // A second pass has nothing left to correct: the first one is a fixed point.
        CHECK(leafDistances::assign(chunk) == 0);
        CHECK(fold(chunk).first == -1372352672412502890LL);
    }

    SUBCASE("bare") {
        Chunk chunk{1, -1};
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 0; z < 16; ++z) {
                chunk.setBlockStateId(x, 60, z, registry::blocks::stone().stateId);
            }
        }
        CHECK(leafDistances::assign(chunk) == 0);
        const auto [checksum, leaves] = fold(chunk);
        CHECK(leaves == 0);
        CHECK(checksum == 7977582471708068901LL);
    }
}
