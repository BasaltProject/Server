#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/BuriedTreasureStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;

namespace {

/// Sandstone on stone under a sea cave: the chest has to walk down past the hollow before it finds a
/// floor it accepts, and the shell then has open sides to fill.
[[nodiscard]] Chunk beachChunk(std::int32_t chunkX, std::int32_t chunkZ, bool hollow) {
    const std::int32_t sandstone = registry::blocks::named("sandstone")->stateId;
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -16; y <= 48; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = 49; y <= 52; ++y) {
                chunk.setBlockStateId(x, y, z, sandstone);
            }
            for (std::int32_t y = 53; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
            // A pocket of water under the sandstone, so the walk down has something to skip.
            if (hollow) {
                for (std::int32_t y = 44; y <= 48; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -16; y <= 66; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::water().stateId &&
                    id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    // The loot seed is the only thing the structure's index in its step reaches the world through,
    // so it goes into the checksum rather than just the count.
    for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
        checksum =
            (checksum ^ static_cast<std::uint64_t>(entity.data().getLong("LootTableSeed", 0))) * 1099511628211ULL;
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t starts = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome, bool hollow, std::int32_t chunks) {
    const std::string biomeName{biome};
    BuriedTreasureStructureGenerator generator{
        [biomeName](std::int32_t, std::int32_t, std::int32_t) { return biomeName; },
        {"minecraft:beach", "minecraft:snowy_beach"},
        [](std::int32_t, std::int32_t) { return 53; }};
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = beachChunk(index % 20 - 10, index / 20 - 10, hollow);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, blockEntities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.blockEntities += blockEntities;
        result.starts += chunk.structures().starts().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t blockEntities, std::size_t starts,
           std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.starts == starts);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the buried treasure sinks where the frozen kotlin sinks it") {
    SUBCASE("beach") { check(run(606, "minecraft:beach", false, 400), 409612, 2, 2, -5449112738805779000LL); }

    SUBCASE("hollow") { check(run(606, "minecraft:beach", true, 400), 409612, 2, 2, 429988634381473224LL); }

    SUBCASE("plains") { check(run(606, "minecraft:plains", false, 400), 409600, 0, 0, 8977839621801090997LL); }

    SUBCASE("otherSeed") { check(run(77, "minecraft:beach", false, 400), 409612, 2, 2, 2853574449254643197LL); }
}
