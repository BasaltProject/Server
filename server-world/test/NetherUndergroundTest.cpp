#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherUndergroundDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration::nether;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Netherrack with soul soil, gravel and caverns, so every support and spring rule can fire.
[[nodiscard]] Chunk netherChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    constexpr std::array<std::int32_t, 7> kThinGaps{30, 35, 45, 50, 90, 95, 100};
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y <= 125; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
            for (std::int32_t y = 20; y <= 23; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("soul_soil"));
            }
            // Open air over the soul soil, or the soul fire and crimson roots have nowhere to go.
            for (std::int32_t y = 24; y <= 26; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
            for (std::int32_t y = 40; y <= 42; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("gravel"));
            }
            for (std::int32_t y = 60; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
            if ((x * 5 + z) % 7 == 0) {
                for (std::int32_t y = 80; y <= 84; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
                }
            }
            // Single-block gaps: only the top block of a cavern has rock above it, and that is the
            // one thing a glowstone blob hangs from.
            for (const std::int32_t y : kThinGaps) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ignored = {"netherrack", "air", "soul_soil", "gravel"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 0; y <= 125; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && !ignored.contains(block->path())) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

[[nodiscard]] terrain::NetherBiomeLookup fixedBiome(std::string_view biome) {
    return [biome](std::int32_t, std::int32_t, std::int32_t, densityfunction::DensityCache*) { return biome; };
}

struct Run {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, std::int32_t index, std::int32_t count) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const terrain::NetherBiomeLookup lookup = fixedBiome(biome);
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12);
        if (index < 0) {
            netherUndergroundDecoration::decorate(chunk, 4141LL + chunkIndex, lookup);
        } else {
            netherUndergroundDecoration::decorateFeature(chunk, 4141LL + chunkIndex, lookup, index);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the nether underground step places what the frozen kotlin places") {
    SUBCASE("deltasStep") {
        const Run result = run("minecraft:basalt_deltas", -1, 4);
        CHECK(result.placed == 66968);
        CHECK(result.deferred == 9);
        CHECK(result.checksum == -6616901302331720368LL);
    }

    SUBCASE("wastesStep") {
        const Run result = run("minecraft:nether_wastes", -1, 4);
        CHECK(result.placed == 2359);
        CHECK(result.deferred == 9);
        CHECK(result.checksum == -3360507228709155986LL);
    }

    SUBCASE("valleyStep") {
        const Run result = run("minecraft:soul_sand_valley", -1, 4);
        CHECK(result.placed == 2729);
        CHECK(result.deferred == 9);
        CHECK(result.checksum == 6974368551250462788LL);
    }

    SUBCASE("index_0") {
        const Run result = run("minecraft:basalt_deltas", 0, 4);
        CHECK(result.placed == 55857);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3536509833262563935LL);
    }

    SUBCASE("index_1") {
        const Run result = run("minecraft:basalt_deltas", 1, 4);
        CHECK(result.placed == 21558);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7407097046079931013LL);
    }

    SUBCASE("index_2") {
        const Run result = run("minecraft:basalt_deltas", 2, 4);
        CHECK(result.placed == 11);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7024972290900274668LL);
    }

    SUBCASE("index_3") {
        const Run result = run("minecraft:nether_wastes", 3, 4);
        CHECK(result.placed == 5);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3600823487218636842LL);
    }

    SUBCASE("index_4") {
        const Run result = run("minecraft:nether_wastes", 4, 4);
        CHECK(result.placed == 37);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7767027590939168945LL);
    }

    SUBCASE("index_5") {
        const Run result = run("minecraft:nether_wastes", 5, 24);
        CHECK(result.placed == 36);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 355257120996722155LL);
    }

    SUBCASE("index_6") {
        const Run result = run("minecraft:nether_wastes", 6, 24);
        CHECK(result.placed == 12);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2815245592675481611LL);
    }

    SUBCASE("index_7") {
        const Run result = run("minecraft:nether_wastes", 7, 4);
        CHECK(result.placed == 41);
        CHECK(result.deferred == 9);
        CHECK(result.checksum == -533791245686742611LL);
    }

    SUBCASE("index_8") {
        const Run result = run("minecraft:soul_sand_valley", 8, 44);
        CHECK(result.placed == 14);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 8027682777876237121LL);
    }

    SUBCASE("index_9") {
        const Run result = run("minecraft:nether_wastes", 9, 24);
        CHECK(result.placed == 74);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8685968852403317339LL);
    }

    SUBCASE("index_10") {
        const Run result = run("minecraft:nether_wastes", 10, 24);
        CHECK(result.placed == 43);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 9034100442582826010LL);
    }

    SUBCASE("index_11") {
        const Run result = run("minecraft:nether_wastes", 11, 4);
        CHECK(result.placed == 1021);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1218822257788054643LL);
    }

    SUBCASE("index_12") {
        const Run result = run("minecraft:basalt_deltas", 12, 4);
        CHECK(result.placed == 59);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 3168572092440956520LL);
    }

    SUBCASE("index_13") {
        const Run result = run("minecraft:basalt_deltas", 13, 4);
        CHECK(result.placed == 421);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4197329621403570751LL);
    }

    SUBCASE("index_14") {
        const Run result = run("minecraft:basalt_deltas", 14, 4);
        CHECK(result.placed == 1218);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 549308592400050409LL);
    }

    SUBCASE("index_15") {
        const Run result = run("minecraft:nether_wastes", 15, 4);
        CHECK(result.placed == 29);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2149719905075186658LL);
    }
}

TEST_CASE("one nether underground feature at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 12> kFeatures{"basalt_blobs", "blackstone_blobs", "brown_mushroom_nether",
                                                         "glowstone",    "glowstone_extra",  "patch_crimson_roots",
                                                         "patch_fire",   "patch_soul_fire",  "red_mushroom_nether",
                                                         "spring_delta", "spring_open",      "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
            Chunk chunk = netherChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(313LL + chunkIndex * 2029LL));
            known += netherUndergroundDecoration::placeAt(chunk, random, chunk.chunkX() * 16 + 8, 59,
                                                          chunk.chunkZ() * 16 + 8, feature)
                         ? "1"
                         : "0";
            const auto [chunkChecksum, chunkPlaced] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            placed += chunkPlaced;
            after = random.nextInt();
        }
    }
    CHECK(known == "111111111111111111111111111111111000");
    CHECK(placed == 1609);
    CHECK(static_cast<std::int64_t>(checksum) == 7329483418435072554LL);
    CHECK(after == 525483368);
}
