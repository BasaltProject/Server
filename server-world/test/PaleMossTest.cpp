#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/PaleMossDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Uneven ground: the patch has to find each column's own surface.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t top = 68 + (x + z) % 3;
            for (std::int32_t y = -64; y < top; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, top, z, stateIdOf((x * 7 + z) % 5 == 0 ? "gravel" : "grass_block"));
        }
    }
    return chunk;
}

struct Fold {
    std::int64_t checksum = 0;
    std::int32_t moss = 0;
    std::int32_t plants = 0;
};

[[nodiscard]] Fold fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Fold result;
    for (std::int32_t y = 60; y <= 90; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr) {
                    continue;
                }
                const std::string path = block->path();
                if (path == "pale_moss_block") {
                    ++result.moss;
                } else if (path == "pale_moss_carpet" || path == "short_grass" || path == "tall_grass") {
                    ++result.plants;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t> run(std::string biome,
                                                                                    std::int32_t count) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t moss = 0;
    std::int32_t plants = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < count; ++index) {
        Chunk chunk = groundChunk(index * 5 - 30, index * -3 + 12);
        const testsupport::FixedBiomeEnvironment environment{biome};
        paleMossDecoration::decorate(chunk, 900LL + index, environment);
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        moss += folded.moss;
        plants += folded.plants;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {moss, plants, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the pale moss patch places what the frozen kotlin places") {
    SUBCASE("paleGarden") {
        const auto [moss, plants, deferred, checksum] = run("minecraft:pale_garden", 20);
        CHECK(moss == 770);
        CHECK(plants == 292);
        CHECK(deferred == 0);
        CHECK(checksum == -1169659330428752451LL);
    }

    SUBCASE("wrongBiome") {
        const auto [moss, plants, deferred, checksum] = run("minecraft:plains", 20);
        CHECK(moss == 0);
        CHECK(plants == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -2334437700212201339LL);
    }
}

TEST_CASE("one pale moss patch matches the frozen kotlin patch by patch") {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t moss = 0;
    std::int32_t plants = 0;
    std::string shape;
    rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(24680LL));
    for (std::int32_t index = 0; index < 12; ++index) {
        Chunk chunk = groundChunk(index, -index);
        paleMossDecoration::placePatchAt(chunk, random,
                                         DecorationPos{.x = index * 16 + 8, .y = 72, .z = -index * 16 + 8});
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        moss += folded.moss;
        plants += folded.plants;
        shape += std::to_string(folded.moss) + ":" + std::to_string(folded.plants) + ",";
    }
    CHECK(moss == 701);
    CHECK(plants == 258);
    CHECK(static_cast<std::int64_t>(checksum) == 2402420495389856988LL);
    CHECK(random.nextInt() == -1677078345);
    CHECK(shape == "74:31,56:23,67:30,43:14,36:18,70:23,54:19,31:10,43:15,87:29,86:26,54:20,");
}
