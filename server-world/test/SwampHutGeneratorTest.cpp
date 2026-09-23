#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/SwampHutStructureGenerator.hpp>

#include <doctest.h>

#include <bit>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;

namespace {

/// Stone under water, so the stilts have to pour down through the swamp to reach the bed.
[[nodiscard]] std::int32_t leaves() {
    static const std::int32_t stateId = registry::blocks::named("oak_leaves")->stateId;
    return stateId;
}

[[nodiscard]] Chunk swampChunk(std::int32_t chunkX, std::int32_t chunkZ, bool slope) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            // An uneven bed, so the average the piece settles on is not the bed itself.
            const std::int32_t rest = (x + z) % 6;
            const std::int32_t top = slope ? 50 + (rest < 0 ? rest + 6 : rest) : 52;
            for (std::int32_t y = 30; y <= top; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = top + 1; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
            // A leaf canopy over the swamp: the average the hut settles on reads the heightmap that
            // ignores leaves, so a canopy must not move it.
            for (std::int32_t y = 64; y <= 65; ++y) {
                chunk.setBlockStateId(x, y, z, leaves());
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = 28; y <= 90; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::water().stateId &&
                    id != leaves() && id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    // The residents' own position, not just how many there are.
    for (const ChunkGeneratedEntity& entity : chunk.generatedEntities()) {
        for (const double value : {entity.x, entity.y, entity.z}) {
            checksum = (checksum ^ static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(value))) * 1099511628211ULL;
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

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome, bool slope, std::int32_t chunks) {
    const std::string biomeName{biome};
    SwampHutStructureGenerator generator{[biomeName](std::int32_t, std::int32_t, std::int32_t) { return biomeName; },
                                         [](std::int32_t, std::int32_t) { return 63; }};
    const auto centre = SwampHutStructureGenerator::grid().candidateChunk(seed, 0, 0);
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = swampChunk(centre.first + index % 6 - 3, centre.second + index / 6 - 3, slope);
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

TEST_CASE("the swamp huts stand where the frozen kotlin stands them") {
    SUBCASE("swamp") { check(run(8080, "minecraft:swamp", true, 36), 175, 2, 1, -5878900164913067243LL); }

    SUBCASE("flat") {
        // A flat bed, so the average the hut settles on is the bed itself.
        check(run(8080, "minecraft:swamp", false, 36), 173, 2, 1, -784352926697723947LL);
    }

    SUBCASE("plains") { check(run(8080, "minecraft:plains", true, 36), 0, 0, 0, 8880054492135905329LL); }

    SUBCASE("otherSeed") { check(run(21, "minecraft:swamp", true, 36), 173, 2, 1, -1654866304175937765LL); }

    SUBCASE("thirdSeed") { check(run(44, "minecraft:swamp", true, 36), 173, 2, 1, 1589025320514197LL); }
}
