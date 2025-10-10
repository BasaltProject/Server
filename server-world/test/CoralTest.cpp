#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CoralDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::FixedBiomeEnvironment;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// A sandy floor under deep water, with a ridge so the ocean floor is not one flat plane.
[[nodiscard]] Chunk oceanChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t floor = 30 + floorMod((chunkX * 16 + x) * 3 + (chunkZ * 16 + z), 4);
            for (std::int32_t y = -64; y <= floor; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("sand"));
            }
            for (std::int32_t y = floor + 1; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

struct Fold {
    std::int64_t checksum = 0;
    std::int32_t coral = 0;
    std::int32_t pickles = 0;
};

[[nodiscard]] Fold fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Fold result;
    for (std::int32_t y = -64; y <= 70; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr) {
                    continue;
                }
                const std::string path = block->path();
                if (path.find("coral") != std::string::npos) {
                    ++result.coral;
                }
                if (path == "sea_pickle") {
                    ++result.pickles;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

struct Run {
    std::int32_t coral = 0;
    std::int32_t pickles = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    for (std::int32_t chunkIndex = 0; chunkIndex < 6; ++chunkIndex) {
        Chunk chunk = oceanChunk(chunkIndex * 2 - 5, chunkIndex * -4 + 7);
        coralDecoration::decorate(chunk, 3389LL + chunkIndex, environment);
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        result.coral += folded.coral;
        result.pickles += folded.pickles;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the warm ocean coral grows the way the frozen kotlin grows it") {
    SUBCASE("warmOcean") {
        const Run result = run("minecraft:warm_ocean");
        CHECK(result.coral == 31);
        CHECK(result.pickles == 2);
        CHECK(result.deferred == 6);
        CHECK(result.checksum == 8040447688267136690LL);
    }

    SUBCASE("coldOcean") {
        const Run result = run("minecraft:cold_ocean");
        CHECK(result.coral == 0);
        CHECK(result.pickles == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -6058816125723604005LL);
    }
}

TEST_CASE("one coral growth at one position matches the frozen kotlin") {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t coral = 0;
    std::int32_t pickles = 0;
    std::int32_t after = 0;
    for (std::int32_t chunkIndex = 0; chunkIndex < 12; ++chunkIndex) {
        Chunk chunk = oceanChunk(chunkIndex, -chunkIndex);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(881LL + chunkIndex * 5077LL));
        coralDecoration::placeAt(chunk, random,
                                 DecorationPos{.x = chunk.chunkX() * 16 + 8, .y = 34, .z = chunk.chunkZ() * 16 + 8});
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        coral += folded.coral;
        pickles += folded.pickles;
        after = random.nextInt();
    }
    CHECK(coral == 453);
    CHECK(pickles == 12);
    CHECK(static_cast<std::int64_t>(checksum) == 2361235567080337202LL);
    CHECK(after == 1275084501);
}

TEST_CASE("the coral vegetal entry matches the frozen kotlin") {
    const BiomeVegetalDecoration step = coralDecoration::vegetalDecoration();
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t coral = 0;
    std::int32_t pickles = 0;
    std::size_t deferred = 0;
    for (std::int32_t chunkIndex = 0; chunkIndex < 6; ++chunkIndex) {
        Chunk chunk = oceanChunk(chunkIndex * 3 - 2, chunkIndex + 1);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(7717LL + chunkIndex));
        step(chunk, random);
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        coral += folded.coral;
        pickles += folded.pickles;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    CHECK(coral == 403);
    CHECK(pickles == 14);
    CHECK(deferred == 312);
    CHECK(static_cast<std::int64_t>(checksum) == 5358181184026242928LL);
}
