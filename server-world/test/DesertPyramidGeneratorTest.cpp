#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/DesertPyramidStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t divisor) {
    const std::int32_t rest = value % divisor;
    return rest < 0 ? rest + divisor : rest;
}

/// A sloped desert, so the lowest column of the footprint is not the one it started on.
[[nodiscard]] std::int32_t surfaceAt(std::int32_t x, std::int32_t z) { return 66 + floorMod(x * 2 + z, 5); }

/// Caves the heightmap knows nothing about, so the columns a pyramid pours down its own footprint
/// have somewhere to fall. Without them the ground right under the base is solid and the pour writes
/// nothing at all.
[[nodiscard]] bool hollow(std::int32_t x, std::int32_t z) { return floorMod(x, 29) < 6 && floorMod(z, 31) < 6; }

[[nodiscard]] std::int32_t terrainAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t surface = surfaceAt(x, z);
    if (y >= surface) {
        return registry::blocks::air().stateId;
    }
    if (hollow(x, z) && y > surface - 20) {
        return registry::blocks::air().stateId;
    }
    return registry::blocks::stone().stateId;
}

[[nodiscard]] Chunk desertChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 30; y <= 90; ++y) {
                chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z));
            }
        }
    }
    return chunk;
}

/// The oracle folds the block entity's type through `String.hashCode`, so the two must agree.
[[nodiscard]] std::int32_t javaHash(std::string_view text) {
    std::int32_t hash = 0;
    for (const char letter : text) {
        hash = static_cast<std::int32_t>(static_cast<std::uint32_t>(hash) * 31U +
                                         static_cast<std::uint32_t>(static_cast<unsigned char>(letter)));
    }
    return hash;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 30; y <= 90; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != terrainAt(chunk.chunkX() * 16 + x, y, chunk.chunkZ() * 16 + z)) {
                    ++written;
                }
            }
        }
    }
    for (const ChunkBlockEntity& entity : chunk.blockEntities()) {
        checksum =
            (checksum ^ static_cast<std::uint64_t>(entity.data().getLong("LootTableSeed", 0))) * 1099511628211ULL;
        checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(javaHash(entity.type().path())))) *
                   1099511628211ULL;
    }
    for (const structure::StructureStartRecord& start : chunk.structures().starts()) {
        for (const std::int32_t bound : {start.fullBounds.minX, start.fullBounds.minY, start.fullBounds.minZ,
                                         start.fullBounds.maxX, start.fullBounds.maxY, start.fullBounds.maxZ}) {
            checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(bound))) * 1099511628211ULL;
        }
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t starts = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome) {
    const std::string biomeName{biome};
    DesertPyramidStructureGenerator generator{
        // Only the centre column of a chunk answers, and only one block under its surface, so where
        // the probe stands and what height it reads are both observable.
        [biomeName](std::int32_t x, std::int32_t y, std::int32_t z) -> std::string {
            const bool centred = (x & 15) == 8 && (z & 15) == 8;
            return centred && y == surfaceAt(x, z) - 1 ? biomeName : std::string{"minecraft:ocean"};
        },
        surfaceAt};
    const auto centre = DesertPyramidStructureGenerator::grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 36; ++index) {
        Chunk chunk = desertChunk(centre.first + index % 6 - 3, centre.second + index / 6 - 3);
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

TEST_CASE("the desert pyramids rise where the frozen kotlin raises them") {
    SUBCASE("desert") { check(run(1234, "minecraft:desert"), 4036, 10, 4, -2680636976981752707LL); }

    SUBCASE("plains") { check(run(1234, "minecraft:plains"), 0, 0, 0, 5232556985271491768LL); }

    SUBCASE("otherSeed") { check(run(77, "minecraft:desert"), 4033, 10, 4, 4262013205564901323LL); }

    SUBCASE("thirdSeed") { check(run(9001, "minecraft:desert"), 4390, 12, 4, 2690890512561099224LL); }

    SUBCASE("fourthSeed") { check(run(4321, "minecraft:desert"), 3910, 12, 4, -539471175363189311LL); }
}
