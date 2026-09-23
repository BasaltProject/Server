#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/OceanMonumentStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

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

[[nodiscard]] Run run(std::int64_t seed, std::int32_t chunks,
                      OceanMonumentStructureGenerator::NoiseBiomeAt noiseBiomeAt) {
    OceanMonumentStructureGenerator generator{std::move(noiseBiomeAt),
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
    SUBCASE("deepOcean") {
        check(run(4711, 144,
                  [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:deep_ocean"}; }),
              14343, 3, 16, 2780290763281437519LL);
    }

    SUBCASE("ocean") {
        check(run(4711, 144, [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:ocean"}; }),
              0, 0, 0, -2536920516572624203LL);
    }

    SUBCASE("plains") {
        check(run(4711, 144, [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:plains"}; }),
              0, 0, 0, -2536920516572624203LL);
    }

    SUBCASE("layered") {
        // Ocean only where the ground probe looks, so the y it is handed is observable.
        check(run(4711, 144,
                  [](std::int32_t, std::int32_t quartY, std::int32_t) {
                      return std::string{quartY == 8 ? "minecraft:deep_ocean" : "minecraft:ocean"};
                  }),
              14343, 3, 16, 2780290763281437519LL);
    }

    SUBCASE("smallPocket") {
        // An ocean pocket wide enough for a radius-24 sweep but not for the real 29, so the monument
        // is refused and a narrower sweep would have let it through.
        const auto centre = OceanMonumentStructureGenerator::grid().candidateChunk(4711, 0, 0);
        const std::int32_t pocketX = (centre.first * 16 + 9) >> 2;
        check(run(4711, 144,
                  [pocketX](std::int32_t quartX, std::int32_t, std::int32_t) {
                      return std::string{std::abs(quartX - pocketX) > 6 ? "minecraft:plains" : "minecraft:deep_ocean"};
                  }),
              0, 0, 0, -2536920516572624203LL);
    }

    SUBCASE("otherSeed") {
        check(run(8, 144, [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:deep_ocean"}; }),
              14447, 3, 16, -8483591205737940600LL);
    }
}
