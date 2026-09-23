#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherOreDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration::nether;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Solid netherrack with a cavern, so the scattered ores' air-exposure rule can bite. No basalt or
/// blackstone bands: blackstone is also what one of the veins places, and terrain of the same block
/// would hide it from the count.
[[nodiscard]] Chunk netherChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y <= 125; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
            for (std::int32_t y = 70; y <= 78; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t ores = 0;
    for (std::int32_t y = 0; y <= 125; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && block->path() != "netherrack" && block->path() != "air") {
                    ++ores;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), ores};
}

[[nodiscard]] terrain::NetherBiomeLookup fixedBiome(std::string_view biome) {
    return [biome](std::int32_t, std::int32_t, std::int32_t, densityfunction::SamplerContext*) { return biome; };
}

struct Run {
    std::int32_t ores = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, std::int32_t index) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const terrain::NetherBiomeLookup lookup = fixedBiome(biome);
    for (std::int32_t chunkIndex = 0; chunkIndex < 16; ++chunkIndex) {
        Chunk chunk = netherChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12);
        if (index < 0) {
            netherOreDecoration::decorate(chunk, 3030LL + chunkIndex, lookup);
        } else {
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(77LL + chunkIndex * 8191LL));
            static_cast<void>(netherOreDecoration::placeFeature(index, chunk, random, lookup));
        }
        const auto [chunkChecksum, chunkOres] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.ores += chunkOres;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the nether ores place what the frozen kotlin places") {
    SUBCASE("wastesStep") {
        const Run result = run("minecraft:nether_wastes", -1);
        CHECK(result.ores == 15400);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 5633885919295166592LL);
    }

    SUBCASE("deltasStep") {
        const Run result = run("minecraft:basalt_deltas", -1);
        CHECK(result.ores == 13897);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 5610227775375956760LL);
    }

    SUBCASE("valleyStep") {
        const Run result = run("minecraft:soul_sand_valley", -1);
        CHECK(result.ores == 17139);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 8935924819677121528LL);
    }

    SUBCASE("feature_ore_magma") {
        const Run result = run("minecraft:nether_wastes", 11);
        CHECK(result.ores == 5875);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5945141373388217390LL);
    }

    SUBCASE("feature_ore_gold_deltas") {
        const Run result = run("minecraft:basalt_deltas", 13);
        CHECK(result.ores == 2320);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -6906282183104394367LL);
    }

    SUBCASE("feature_ore_quartz_deltas") {
        const Run result = run("minecraft:basalt_deltas", 14);
        CHECK(result.ores == 6491);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7229661945940144687LL);
    }

    SUBCASE("feature_ore_soul_sand") {
        const Run result = run("minecraft:soul_sand_valley", 16);
        CHECK(result.ores == 1953);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7523478457456584196LL);
    }

    SUBCASE("feature_ore_gravel_nether") {
        const Run result = run("minecraft:nether_wastes", 17);
        CHECK(result.ores == 3052);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1146763406075752339LL);
    }

    SUBCASE("feature_ore_blackstone") {
        const Run result = run("minecraft:nether_wastes", 18);
        CHECK(result.ores == 3035);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7101798105624700642LL);
    }

    SUBCASE("feature_ore_gold_nether") {
        const Run result = run("minecraft:nether_wastes", 19);
        CHECK(result.ores == 1125);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1459259452646051212LL);
    }

    SUBCASE("feature_ore_quartz_nether") {
        const Run result = run("minecraft:nether_wastes", 20);
        CHECK(result.ores == 3306);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1622531430782250779LL);
    }

    SUBCASE("feature_ore_ancient_debris_large") {
        const Run result = run("minecraft:nether_wastes", 21);
        CHECK(result.ores == 13);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1517337849913115378LL);
    }

    SUBCASE("feature_ore_debris_small") {
        const Run result = run("minecraft:nether_wastes", 22);
        CHECK(result.ores == 9);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2780260854113537014LL);
    }
}

TEST_CASE("one nether ore at one position matches the frozen kotlin") {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t ores = 0;
    std::string known;
    std::int32_t after = 0;
    for (const auto& [name, index] : netherOreDecoration::placedIds()) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 4; ++chunkIndex) {
            Chunk chunk = netherChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(151LL + chunkIndex * 613LL));
            known +=
                netherOreDecoration::placeAt(chunk, random, chunk.chunkX() * 16 + 8, 60, chunk.chunkZ() * 16 + 8, name)
                    ? "1"
                    : "0";
            const auto [chunkChecksum, chunkOres] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            ores += chunkOres;
            after = random.nextInt();
        }
    }
    Chunk chunk = netherChunk(0, 0);
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(1LL));
    known += netherOreDecoration::placeAt(chunk, random, 8, 60, 8, "not_an_ore") ? "1" : "0";
    CHECK(known == "11111111111111111111111111111111111111110");
    CHECK(ores == 1540);
    CHECK(static_cast<std::int64_t>(checksum) == -6927862987991074372LL);
    CHECK(after == -2093014969);
}
