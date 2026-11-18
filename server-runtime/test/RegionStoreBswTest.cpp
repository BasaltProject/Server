#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/metamorph/region/BswConvert.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/RegionStore.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::config::RegionFormatSetting;
using basalt::server::world::Chunk;
namespace fs = std::filesystem;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const fs::path directory = fs::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    fs::remove_all(directory, ignored);
    return directory.string();
}

[[nodiscard]] Chunk filled(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t height) {
    const registry::BlockState* block = registry::blocks::named("stone");
    REQUIRE(block != nullptr);
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, height, z, block->stateId);
        }
    }
    return chunk;
}

void saveAll(RegionStore& store, std::vector<Chunk>& chunks) {
    std::vector<Chunk*> pointers;
    for (Chunk& chunk : chunks) {
        pointers.push_back(&chunk);
    }
    store.saveChunks(pointers, 1000);
}

[[nodiscard]] std::vector<Chunk> sample() {
    std::vector<Chunk> chunks;
    for (std::int32_t at = 0; at < 40; ++at) {
        chunks.push_back(filled(at - 20, at % 7, 60 + at % 5));
    }
    return chunks;
}

void requireAll(RegionStore& store, const std::vector<Chunk>& chunks) {
    for (const Chunk& chunk : chunks) {
        const auto loaded = store.loadChunk(chunk.chunkX(), chunk.chunkZ());
        REQUIRE(loaded.has_value());
        CHECK(loaded->blockStateId(3, 60 + (chunk.chunkX() + 20) % 5, 3) ==
              chunk.blockStateId(3, 60 + (chunk.chunkX() + 20) % 5, 3));
    }
}

} // namespace

TEST_CASE("a new world defaults to bsw and keeps its chunks across a restart and a batched stop") {
    const std::string root = scratch("bsw-store");
    std::vector<Chunk> chunks = sample();
    {
        RegionStore store{"world",
                          root,
                          basalt::server::world::kOverworldHeight,
                          12,
                          RegionStore::kByteBudget,
                          RegionFormatSetting::Auto};
        REQUIRE(store.usesBsw());
        saveAll(store, chunks);
        store.beginBatch();
        std::vector<Chunk> more;
        more.push_back(filled(100, 100, 62));
        saveAll(store, more);
        CHECK(store.hasChunk(100, 100));
        CHECK(store.flush() > 0);
    }
    CHECK(fs::exists(fs::path{root} / "world" / "region" / "r.3.3.bsw"));
    RegionStore reopened{"world",
                         root,
                         basalt::server::world::kOverworldHeight,
                         12,
                         RegionStore::kByteBudget,
                         RegionFormatSetting::Auto};
    REQUIRE(reopened.usesBsw());
    requireAll(reopened, chunks);
    CHECK(reopened.hasChunk(100, 100));
}

TEST_CASE("an existing linear world converts to bsw and back, verified, keeping the originals") {
    const std::string root = scratch("bsw-convert");
    std::vector<Chunk> chunks = sample();
    {
        RegionStore store{"world", root};
        REQUIRE_FALSE(store.usesBsw());
        saveAll(store, chunks);
    }
    {
        RegionStore autoStore{"world",
                              root,
                              basalt::server::world::kOverworldHeight,
                              12,
                              RegionStore::kByteBudget,
                              RegionFormatSetting::Auto};
        CHECK_FALSE(autoStore.usesBsw());
    }
    {
        RegionStore store{"world",
                          root,
                          basalt::server::world::kOverworldHeight,
                          12,
                          RegionStore::kByteBudget,
                          RegionFormatSetting::Bsw};
        REQUIRE(store.usesBsw());
        requireAll(store, chunks);
    }
    CHECK(fs::exists(fs::path{root} / "world" / "region.linear-original"));
    {
        RegionStore store{"world",
                          root,
                          basalt::server::world::kOverworldHeight,
                          12,
                          RegionStore::kByteBudget,
                          RegionFormatSetting::Linear};
        REQUIRE_FALSE(store.usesBsw());
        requireAll(store, chunks);
    }
    CHECK(fs::exists(fs::path{root} / "world" / "region.bsw-original"));
    {
        RegionStore store{"world",
                          root,
                          basalt::server::world::kOverworldHeight,
                          12,
                          RegionStore::kByteBudget,
                          RegionFormatSetting::Anvil};
        REQUIRE(store.usesAnvil());
        requireAll(store, chunks);
        saveAll(store, chunks);
    }
    CHECK(fs::exists(fs::path{root} / "world" / "region.linear-original-2"));
    {
        RegionStore store{"world",
                          root,
                          basalt::server::world::kOverworldHeight,
                          12,
                          RegionStore::kByteBudget,
                          RegionFormatSetting::Anvil};
        requireAll(store, chunks);
    }
    {
        RegionStore store{"world",
                          root,
                          basalt::server::world::kOverworldHeight,
                          12,
                          RegionStore::kByteBudget,
                          RegionFormatSetting::Linear};
        REQUIRE_FALSE(store.usesAnvil());
        requireAll(store, chunks);
    }
    fs::create_directories(fs::path{root} / "world" / "region");
    std::ofstream{fs::path{root} / "world" / "region" / "r.9.9.bsw"} << "x";
    CHECK_THROWS(RegionStore("world", root, basalt::server::world::kOverworldHeight, 12, RegionStore::kByteBudget,
                             RegionFormatSetting::Auto));
}

TEST_CASE("a corrupt bsw chunk is reported and never overwritten") {
    const std::string root = scratch("bsw-corrupt");
    std::vector<Chunk> chunks;
    chunks.push_back(filled(1, 1, 60));
    {
        RegionStore store{"world",
                          root,
                          basalt::server::world::kOverworldHeight,
                          12,
                          RegionStore::kByteBudget,
                          RegionFormatSetting::Auto};
        saveAll(store, chunks);
    }
    const fs::path file = fs::path{root} / "world" / "region" / "r.0.0.bsw";
    {
        std::fstream out{file, std::ios::in | std::ios::out | std::ios::binary};
        out.seekp(77824 + 9);
        const char junk = 0x7F;
        out.write(&junk, 1);
    }
    RegionStore store{"world",
                      root,
                      basalt::server::world::kOverworldHeight,
                      12,
                      RegionStore::kByteBudget,
                      RegionFormatSetting::Auto};
    CHECK_FALSE(store.loadChunk(1, 1).has_value());
    std::vector<Chunk> fresh;
    fresh.push_back(filled(1, 1, 70));
    saveAll(store, fresh);
    CHECK_FALSE(store.loadChunk(1, 1).has_value());
}

TEST_CASE("bsw keeps binary chunks yet serves and accepts NBT through the raw calls") {
    const std::string root = scratch("bsw-raw");
    std::vector<Chunk> chunks;
    chunks.push_back(filled(2, 3, 70));
    RegionStore store{"world",
                      root,
                      basalt::server::world::kOverworldHeight,
                      12,
                      RegionStore::kByteBudget,
                      RegionFormatSetting::Auto};
    saveAll(store, chunks);
    const auto raw = store.rawChunk(2, 3);
    REQUIRE(raw.has_value());
    CHECK((*raw)[0] == 10);
    REQUIRE(store.writeRawChunk(5, 6, *raw, 2000));
    const auto copied = store.loadChunk(5, 6);
    REQUIRE(copied.has_value());
    CHECK(copied->blockStateId(3, 70, 3) == chunks[0].blockStateId(3, 70, 3));
}
