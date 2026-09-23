#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherLocalDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration::nether;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// A soul sand valley roof over an open cavern, which is the only shape a pillar hangs from.
[[nodiscard]] Chunk valleyChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 10; y <= 20; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("soul_soil"));
            }
            for (std::int32_t y = 60; y <= 80; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t basalt = 0;
    for (std::int32_t y = 0; y <= 85; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && block->path() == "basalt") {
                    ++basalt;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), basalt};
}

[[nodiscard]] terrain::NetherBiomeLookup fixedBiome(std::string_view biome) {
    return [biome](std::int32_t, std::int32_t, std::int32_t, densityfunction::SamplerContext*) { return biome; };
}

} // namespace

TEST_CASE("the nether basalt pillar builds what the frozen kotlin builds") {
    SUBCASE("valley") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t basalt = 0;
        std::size_t deferred = 0;
        for (std::int32_t index = 0; index < 24; ++index) {
            Chunk chunk = valleyChunk(index * 5 - 30, index * -3 + 12);
            netherLocalDecoration::decorate(chunk, 606LL + index, fixedBiome("minecraft:soul_sand_valley"));
            const auto [chunkChecksum, chunkBasalt] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            basalt += chunkBasalt;
            deferred += chunk.takeGeneratedBlockWrites().size();
        }
        CHECK(basalt == 332);
        CHECK(deferred == 0);
        CHECK(static_cast<std::int64_t>(checksum) == 8757461921920074297LL);
    }

    SUBCASE("wastes") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t basalt = 0;
        std::size_t deferred = 0;
        for (std::int32_t index = 0; index < 24; ++index) {
            Chunk chunk = valleyChunk(index * 5 - 30, index * -3 + 12);
            netherLocalDecoration::decorate(chunk, 606LL + index, fixedBiome("minecraft:nether_wastes"));
            const auto [chunkChecksum, chunkBasalt] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            basalt += chunkBasalt;
            deferred += chunk.takeGeneratedBlockWrites().size();
        }
        CHECK(basalt == 0);
        CHECK(deferred == 0);
        CHECK(static_cast<std::int64_t>(checksum) == 5535535463266102621LL);
    }

    SUBCASE("pillarAt") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t basalt = 0;
        std::int32_t after = 0;
        for (std::int32_t index = 0; index < 24; ++index) {
            Chunk chunk = valleyChunk(index * 5 - 30, index * -3 + 12);
            rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(909LL + index * 3121LL));
            netherLocalDecoration::placeAt(chunk, random, chunk.chunkX() * 16 + 8, 59, chunk.chunkZ() * 16 + 8);
            const auto [chunkChecksum, chunkBasalt] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            basalt += chunkBasalt;
            after = random.nextInt();
        }
        CHECK(basalt == 2597);
        CHECK(static_cast<std::int64_t>(checksum) == 2382979694484101278LL);
        CHECK(after == 2098350802);
    }
}
