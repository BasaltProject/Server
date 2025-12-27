#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MineshaftFill.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::structure;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

/// Stone with a water pocket well away from the shaft, so the invalid-location scan can bite.
[[nodiscard]] Chunk stoneChunk(std::int32_t chunkX, std::int32_t chunkZ, bool flooded) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -8; y <= 60; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            if (flooded) {
                chunk.setBlockStateId(x, 30, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -8; y <= 64; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t placed = 0;
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t entities = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, MineshaftType type, bool flooded, std::string_view biome,
                      std::int32_t shafts) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < shafts; ++index) {
        const std::int32_t chunkX = index * 7 - 20;
        const std::int32_t chunkZ = index * -3 + 11;
        const std::vector<MineshaftPiece> pieces =
            mineshaftLayout::assemble(seed + index, chunkX, chunkZ, type, 63, -64).pieces;
        Chunk chunk = stoneChunk(chunkX, chunkZ, flooded);
        const StructureBoundingBox clip{chunkX * 16, -8, chunkZ * 16, chunkX * 16 + 15, 64, chunkZ * 16 + 15};
        const MineshaftBlocks blocks{type};
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(seed + index));
        for (const MineshaftPiece& piece : pieces) {
            if (!piece.box.intersects(clip)) {
                continue;
            }
            ++result.placed;
            mineshaftFill::place(piece, blocks, chunk, clip, random,
                                 [biome](std::int32_t, std::int32_t, std::int32_t) { return std::string{biome}; });
        }
        const auto [chunkChecksum, chunkWritten, chunkBlockEntities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += chunkWritten;
        result.blockEntities += chunkBlockEntities;
        result.entities += chunk.generatedEntities().size();
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t placed, std::int32_t written, std::size_t blockEntities,
           std::size_t entities, std::size_t deferred, std::int64_t checksum) {
    CHECK(result.placed == placed);
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.entities == entities);
    CHECK(result.deferred == deferred);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the mineshaft fill writes what the frozen kotlin writes") {
    SUBCASE("normal") {
        check(run(4242, MineshaftType::Normal, false, "minecraft:plains", 40), 244, 22655, 2, 5, 0,
              1657795336415617398LL);
    }

    SUBCASE("mesa") {
        check(run(4242, MineshaftType::Mesa, false, "minecraft:badlands", 40), 390, 36630, 4, 4, 0,
              -3355036388178176706LL);
    }

    SUBCASE("flooded") {
        check(run(4242, MineshaftType::Normal, true, "minecraft:plains", 40), 244, 27505, 3, 2, 0,
              1807300684606125337LL);
    }

    SUBCASE("deepDark") {
        check(run(4242, MineshaftType::Normal, false, "minecraft:deep_dark", 40), 244, 0, 0, 0, 0,
              -7241813388479369619LL);
    }
}
