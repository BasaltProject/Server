#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherSurfaceStructuresDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration::nether;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// An uneven netherrack floor with lava below the Nether sea level and open air over it.
[[nodiscard]] Chunk deltaChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t top = 28 + (x * 3 + z) % 4;
            for (std::int32_t y = 0; y <= top; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
            if ((x + z) % 6 == 0) {
                for (std::int32_t y = 26; y <= 30; ++y) {
                    chunk.setBlockStateId(x, y, z, stateIdOf("lava"));
                }
            }
            for (std::int32_t y = 60; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
        }
    }
    return chunk;
}

struct Fold {
    std::int64_t checksum = 0;
    std::int32_t basalt = 0;
    std::int32_t magma = 0;
};

[[nodiscard]] Fold fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Fold result;
    for (std::int32_t y = 0; y <= 70; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr) {
                    continue;
                }
                if (block->path() == "basalt") {
                    ++result.basalt;
                } else if (block->path() == "magma_block") {
                    ++result.magma;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] terrain::NetherBiomeLookup fixedBiome(std::string_view biome) {
    return [biome](std::int32_t, std::int32_t, std::int32_t, densityfunction::DensityCache*) { return biome; };
}

[[nodiscard]] Fold run(std::string_view biome, std::int32_t index) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Fold result;
    const terrain::NetherBiomeLookup lookup = fixedBiome(biome);
    for (std::int32_t chunkIndex = 0; chunkIndex < 12; ++chunkIndex) {
        Chunk chunk = deltaChunk(chunkIndex * 5 - 30, chunkIndex * -3 + 12);
        if (index < 0) {
            netherSurfaceStructuresDecoration::decorate(chunk, 5150LL + chunkIndex, lookup);
        } else {
            netherSurfaceStructuresDecoration::decorateFeature(chunk, 5150LL + chunkIndex, lookup, index);
        }
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        result.basalt += folded.basalt;
        result.magma += folded.magma;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the basalt deltas surface features build what the frozen kotlin builds") {
    SUBCASE("deltasStep") {
        const Fold result = run("minecraft:basalt_deltas", -1);
        CHECK(result.basalt == 8737);
        CHECK(result.magma == 119);
        CHECK(result.checksum == -3000954630696362016LL);
    }

    SUBCASE("wastesStep") {
        const Fold result = run("minecraft:nether_wastes", -1);
        CHECK(result.basalt == 0);
        CHECK(result.magma == 0);
        CHECK(result.checksum == 7868481725456217217LL);
    }

    SUBCASE("delta") {
        const Fold result = run("minecraft:basalt_deltas", 0);
        CHECK(result.basalt == 0);
        CHECK(result.magma == 119);
        CHECK(result.checksum == 3538581714467077477LL);
    }

    SUBCASE("smallColumns") {
        const Fold result = run("minecraft:basalt_deltas", 1);
        CHECK(result.basalt == 724);
        CHECK(result.magma == 0);
        CHECK(result.checksum == 1399069255947750473LL);
    }

    SUBCASE("largeColumns") {
        const Fold result = run("minecraft:basalt_deltas", 2);
        CHECK(result.basalt == 8563);
        CHECK(result.magma == 0);
        CHECK(result.checksum == 6706969506979995744LL);
    }
}

TEST_CASE("one basalt deltas feature at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 4> kFeatures{"delta", "small_basalt_columns", "large_basalt_columns",
                                                        "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t basalt = 0;
    std::int32_t magma = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 6; ++chunkIndex) {
            Chunk chunk = deltaChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(24LL + chunkIndex * 4099LL));
            // The floor at (8, 8) tops out at 28, so this is the first air above it.
            known += netherSurfaceStructuresDecoration::placeAt(chunk, random, chunk.chunkX() * 16 + 8, 29,
                                                                chunk.chunkZ() * 16 + 8, feature)
                         ? "1"
                         : "0";
            const Fold folded = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
            basalt += folded.basalt;
            magma += folded.magma;
            after = random.nextInt();
        }
    }
    CHECK(known == "111111111111111111000000");
    CHECK(basalt == 4565);
    CHECK(magma == 28);
    CHECK(static_cast<std::int64_t>(checksum) == 6862765596178118547LL);
    CHECK(after == -1601777976);
}
