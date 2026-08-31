#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>

#include <doctest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// A chunk whose surface height encodes its own coordinates, so a read proves which neighbour
/// answered rather than only that something did.
[[nodiscard]] Chunk codedChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    const std::int32_t top = 64 + ((chunkX * 3 + chunkZ * 5) & 7);
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y <= top; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

} // namespace

TEST_CASE("a bounded view answers inside its radius and refuses outside it") {
    std::atomic<std::int32_t> generated{0};
    NeighbourTerrainCache cache{[&generated](std::int32_t chunkX, std::int32_t chunkZ) {
        ++generated;
        return codedChunk(chunkX, chunkZ);
    }};
    const std::unique_ptr<NeighbourTerrain> view = cache.around(0, 0);

    // One chunk away is in reach; two is not, whatever the block there would have been.
    CHECK(view->blockStateIdAt(-1, 62, 5).value() == registry::blocks::stone().stateId);
    CHECK(view->blockStateIdAt(20, 62, 5).value() == registry::blocks::stone().stateId);
    CHECK_FALSE(view->blockStateIdAt(40, 62, 5).has_value());
    CHECK_FALSE(view->oceanFloorHeightAt(-40, 5).has_value());

    // Out of the world vertically is a refusal, not a wrong block.
    CHECK_FALSE(view->blockStateIdAt(-1, 400, 5).has_value());

    CHECK(view->oceanFloorHeightAt(-1, 5).value() == 64 + ((-1 * 3 + 0 * 5) & 7) + 1);
    CHECK(view->oceanFloorHeightAt(20, 5).value() == 64 + ((1 * 3 + 0 * 5) & 7) + 1);

    // Two chunks were needed and each was generated once, however many reads they took.
    CHECK(generated.load() == 2);
    static_cast<void>(view->blockStateIdAt(-1, 63, 6));
    static_cast<void>(view->oceanFloorHeightAt(-1, 6));
    CHECK(generated.load() == 2);

    cache.clear();
    CHECK(view->blockStateIdAt(-1, 62, 5).value() == registry::blocks::stone().stateId);
    CHECK(generated.load() == 3);
}

TEST_CASE("the cache drops its oldest neighbour once it is full") {
    std::atomic<std::int32_t> generated{0};
    NeighbourTerrainCache cache{[&generated](std::int32_t chunkX, std::int32_t chunkZ) {
                                    ++generated;
                                    return codedChunk(chunkX, chunkZ);
                                },
                                8, 2};
    const std::unique_ptr<NeighbourTerrain> view = cache.around(0, 0);
    static_cast<void>(view->oceanFloorHeightAt(0, 0));
    static_cast<void>(view->oceanFloorHeightAt(16, 0));
    static_cast<void>(view->oceanFloorHeightAt(32, 0));
    CHECK(generated.load() == 3);
    // The first is gone and comes back generated; the newest two are still held.
    static_cast<void>(view->oceanFloorHeightAt(0, 0));
    CHECK(generated.load() == 4);
    static_cast<void>(view->oceanFloorHeightAt(32, 0));
    CHECK(generated.load() == 4);
}

TEST_CASE("concurrent readers of the same neighbour agree") {
    std::atomic<std::int32_t> generated{0};
    NeighbourTerrainCache cache{[&generated](std::int32_t chunkX, std::int32_t chunkZ) {
        ++generated;
        return codedChunk(chunkX, chunkZ);
    }};
    std::atomic<std::int32_t> mismatches{0};
    std::vector<std::jthread> workers;
    workers.reserve(4);
    for (std::int32_t worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&cache, &mismatches] {
            const std::unique_ptr<NeighbourTerrain> view = cache.around(0, 0);
            for (std::int32_t attempt = 0; attempt < 200; ++attempt) {
                const std::int32_t chunkX = attempt % 3 - 1;
                const std::int32_t expected = 64 + ((chunkX * 3) & 7) + 1;
                if (view->oceanFloorHeightAt(chunkX * 16 + 4, 4).value() != expected) {
                    ++mismatches;
                }
            }
        });
    }
    workers.clear();
    CHECK(mismatches.load() == 0);
}
