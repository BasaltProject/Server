#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/metamorph/region/LinearRegion.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/RegionStore.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
namespace fileIO = basalt::wire::io::fileIO;
namespace registry = basalt::server::registry;
using basalt::server::world::Chunk;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::int32_t stoneState() {
    const registry::BlockState* block = registry::blocks::named("stone");
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] Chunk filled(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t height) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, height, z, stoneState());
        }
    }
    return chunk;
}

[[nodiscard]] std::size_t save(RegionStore& store, std::vector<Chunk>& chunks, std::int32_t timestamp) {
    std::vector<Chunk*> pointers;
    pointers.reserve(chunks.size());
    for (Chunk& chunk : chunks) {
        pointers.push_back(&chunk);
    }
    return store.saveChunks(pointers, timestamp);
}

} // namespace

TEST_CASE("a chunk saved is a chunk read back") {
    const std::string root = scratch("region-store");
    RegionStore store{"world", root};
    CHECK(store.worldRoot() == root + "/world");
    CHECK(store.regionPath(0, 0) == root + "/world/region/r.0.0.linear");
    CHECK(store.regionPath(-2, 3) == root + "/world/region/r.-2.3.linear");

    CHECK_FALSE(store.loadChunk(0, 0).has_value());

    std::vector<Chunk> chunks;
    chunks.push_back(filled(0, 0, 70));
    CHECK(save(store, chunks, 1234) == 1);
    CHECK(fileIO::exists(store.regionPath(0, 0)));
    CHECK_FALSE(chunks[0].isDirty());

    const std::optional<Chunk> read = store.loadChunk(0, 0);
    REQUIRE(read.has_value());
    CHECK(read->chunkX() == 0);
    CHECK(read->chunkZ() == 0);
    CHECK(read->blockStateId(5, 70, 5) == stoneState());
    CHECK(read->blockStateId(5, 71, 5) == registry::blocks::air().stateId);
    CHECK_FALSE(read->isDirty());
}

TEST_CASE("chunks land in the region they belong to, negative coordinates included") {
    const std::string root = scratch("region-store-coords");
    RegionStore store{"world", root};

    std::vector<Chunk> chunks;
    chunks.push_back(filled(0, 0, 64));
    chunks.push_back(filled(31, 31, 65));
    chunks.push_back(filled(32, 0, 66));
    chunks.push_back(filled(-1, -1, 67));
    CHECK(save(store, chunks, 10) == 3);
    CHECK(fileIO::exists(store.regionPath(0, 0)));
    CHECK(fileIO::exists(store.regionPath(1, 0)));
    CHECK(fileIO::exists(store.regionPath(-1, -1)));

    CHECK(store.loadChunk(31, 31)->blockStateId(0, 65, 0) == stoneState());
    CHECK(store.loadChunk(32, 0)->blockStateId(0, 66, 0) == stoneState());
    CHECK(store.loadChunk(-1, -1)->blockStateId(0, 67, 0) == stoneState());
    CHECK_FALSE(store.loadChunk(-2, -2).has_value());
}

TEST_CASE("a region west of the origin is not the region north of it") {
    const std::string root = scratch("region-store-negative");
    RegionStore store{"world", root};

    std::vector<Chunk> chunks;
    chunks.push_back(filled(-1, 0, 64));
    chunks.push_back(filled(-1, -1, 65));
    CHECK(save(store, chunks, 1) == 2);
    CHECK(fileIO::exists(store.regionPath(-1, 0)));
    CHECK(fileIO::exists(store.regionPath(-1, -1)));
    CHECK(store.loadChunk(-1, 0)->blockStateId(0, 64, 0) == stoneState());
    CHECK(store.loadChunk(-1, -1)->blockStateId(0, 65, 0) == stoneState());
}

TEST_CASE("the timestamp a save was given is the one in the file") {
    const std::string root = scratch("region-store-timestamp");
    RegionStore store{"world", root};

    std::vector<Chunk> chunks;
    chunks.push_back(filled(1, 2, 64));
    CHECK(save(store, chunks, 1700000000) == 1);

    const std::optional<std::vector<std::uint8_t>> bytes = fileIO::readBytes(store.regionPath(0, 0));
    REQUIRE(bytes.has_value());
    const std::expected<basalt::metamorph::region::LinearRegion, basalt::metamorph::region::LinearError> region =
        basalt::metamorph::region::LinearRegion::read(*bytes);
    REQUIRE(region.has_value());
    CHECK(region->timestampAt(1, 2) == 1700000000);
}

TEST_CASE("the region kept is the one most recently used") {
    const std::string root = scratch("region-store-lru");
    RegionStore store{"world", root, basalt::server::world::kOverworldHeight, 2, 1024LL * 1024 * 1024};

    for (std::int32_t region = 0; region < 2; ++region) {
        std::vector<Chunk> one;
        one.push_back(filled(region * 32, 0, 64));
        CHECK(save(store, one, 1) == 1);
    }
    CHECK(store.loadChunk(0, 0).has_value());

    std::vector<Chunk> third;
    third.push_back(filled(64, 0, 64));
    CHECK(save(store, third, 1) == 1);
    CHECK(store.cachedRegionCount() == 2);

    CHECK(fileIO::remove(store.regionPath(0, 0)));
    CHECK(fileIO::remove(store.regionPath(1, 0)));
    CHECK(store.loadChunk(0, 0).has_value());
    CHECK_FALSE(store.loadChunk(32, 0).has_value());
}

TEST_CASE("a region file is rewritten whole, however few of its chunks changed") {
    const std::string root = scratch("region-store-rewrite");
    RegionStore store{"world", root};

    std::vector<Chunk> first;
    for (std::int32_t at = 0; at < 8; ++at) {
        first.push_back(filled(at, 0, 64 + at));
    }
    CHECK(save(store, first, 1) == 1);

    std::vector<Chunk> second;
    second.push_back(filled(3, 0, 100));
    CHECK(save(store, second, 2) == 1);

    CHECK(store.loadChunk(5, 0)->blockStateId(0, 69, 0) == stoneState());
    CHECK(store.loadChunk(3, 0)->blockStateId(0, 100, 0) == stoneState());
    CHECK(store.loadChunk(3, 0)->blockStateId(0, 67, 0) == registry::blocks::air().stateId);
}

TEST_CASE("saving nothing writes nothing") {
    const std::string root = scratch("region-store-empty");
    RegionStore store{"world", root};
    std::vector<Chunk> none;
    CHECK(save(store, none, 1) == 0);
    CHECK_FALSE(fileIO::exists(store.regionPath(0, 0)));
    CHECK(store.flush() == 0);
}

TEST_CASE("the cache keeps a few regions and lets the rest go") {
    const std::string root = scratch("region-store-cache");
    RegionStore store{"world", root, basalt::server::world::kOverworldHeight, 2, 1024LL * 1024 * 1024};

    std::vector<Chunk> chunks;
    for (std::int32_t region = 0; region < 4; ++region) {
        chunks.push_back(filled(region * 32, 0, 64));
    }
    CHECK(save(store, chunks, 1) == 4);
    CHECK(store.cachedRegionCount() == 4);

    std::vector<Chunk> later;
    later.push_back(filled(4 * 32, 0, 64));
    CHECK(save(store, later, 2) == 1);
    CHECK(store.cachedRegionCount() <= 2);
    CHECK(store.cachedRegionBytes() > 0);

    for (std::int32_t region = 0; region < 5; ++region) {
        CAPTURE(region);
        CHECK(store.loadChunk(region * 32, 0).has_value());
    }
}

TEST_CASE("a region with unsaved writes stays cached however small the budget") {
    const std::string root = scratch("region-store-dirty");
    RegionStore store{"world", root, basalt::server::world::kOverworldHeight, 1, 1};

    std::vector<Chunk> chunks;
    for (std::int32_t region = 0; region < 3; ++region) {
        chunks.push_back(filled(region * 32, 0, 64));
    }
    CHECK(save(store, chunks, 1) == 3);
    for (std::int32_t region = 0; region < 3; ++region) {
        CAPTURE(region);
        CHECK(store.loadChunk(region * 32, 0).has_value());
    }
}

TEST_CASE("what a save spent, split into the two halves an operator can act on") {
    const std::string root = scratch("region-store-timings");
    RegionStore store{"world", root};
    CHECK(store.encodeMillis() == 0);
    CHECK(store.writeMillis() == 0);

    std::vector<Chunk> chunks;
    for (std::int32_t at = 0; at < 64; ++at) {
        chunks.push_back(filled(at % 32, at / 32, 64));
    }
    CHECK(save(store, chunks, 1) > 0);
    CHECK(store.encodeMillis() > 0);
    CHECK(store.writeMillis() >= 0);
}

TEST_CASE("a region file that is not one reads as an unsaved chunk") {
    const std::string root = scratch("region-store-corrupt");
    RegionStore store{"world", root};
    fileIO::createDirectories(root + "/world/region");
    fileIO::writeBytes(store.regionPath(0, 0), std::vector<std::uint8_t>(64, 0));

    CHECK_FALSE(store.loadChunk(0, 0).has_value());
}

TEST_CASE("closed region storage cannot recreate a removed world") {
    const std::string root = scratch("region-store-closed");
    RegionStore store{"temporary", root};
    std::vector<Chunk> chunks;
    chunks.push_back(filled(0, 0, 64));
    REQUIRE(save(store, chunks, 1) > 0);
    store.close();
    std::filesystem::remove_all(root + "/temporary");
    chunks[0].setBlockStateId(0, 65, 0, stoneState());
    CHECK(save(store, chunks, 2) == 0);
    CHECK(store.flush() == 0);
    CHECK_FALSE(store.loadChunk(0, 0).has_value());
    CHECK_FALSE(std::filesystem::exists(root + "/temporary"));
}
