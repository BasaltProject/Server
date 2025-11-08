#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/OceanMonumentStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk seaChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -16; y <= 35; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = 36; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -16; y <= 70; ++y) {
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
    return {static_cast<std::int64_t>(checksum), written, chunk.generatedEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t entities = 0;
    std::size_t starts = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome, std::int32_t chunks) {
    const std::string biomeName{biome};
    OceanMonumentStructureGenerator generator{
        [biomeName](std::int32_t, std::int32_t, std::int32_t) { return biomeName; },
        {"minecraft:deep_ocean", "minecraft:deep_cold_ocean"},
        {"minecraft:deep_ocean", "minecraft:deep_cold_ocean", "minecraft:ocean"},
        [](std::int32_t, std::int32_t) { return 36; }};
    // Centred on the candidate the grid picks for the middle cell, which is 32 chunks wide.
    const auto centre = OceanMonumentStructureGenerator::grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = seaChunk(centre.first + index % 12 - 6, centre.second + index / 12 - 6);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, entities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.entities += entities;
        result.starts += chunk.structures().starts().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t entities, std::size_t starts, std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.entities == entities);
    CHECK(result.starts == starts);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the ocean monuments stand where the frozen kotlin puts them") {
    SUBCASE("deepOcean") { check(run(4711, "minecraft:deep_ocean", 144), 14350, 3, 16, 1448401856749821290LL); }

    SUBCASE("ocean") { check(run(4711, "minecraft:ocean", 144), 0, 0, 0, 1153227349664774837LL); }

    SUBCASE("plains") { check(run(4711, "minecraft:plains", 144), 0, 0, 0, 1153227349664774837LL); }

    SUBCASE("otherSeed") { check(run(8, "minecraft:deep_ocean", 144), 14464, 3, 16, -2931064608776548685LL); }
}
