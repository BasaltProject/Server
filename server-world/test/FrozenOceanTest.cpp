#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FrozenOceanDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>

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

/// Deep water over gravel, an ice sheet on top, and packed ice floes for the blue ice to grow on.
[[nodiscard]] Chunk oceanChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            // Only the band the icebergs and the blue ice can reach is filled; the rest stays air.
            for (std::int32_t y = 28; y <= 40; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("gravel"));
            }
            for (std::int32_t y = 41; y <= 62; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
            if (floorMod(worldX * 5 + worldZ, 3) != 0) {
                chunk.setBlockStateId(x, 63, z, stateIdOf("ice"));
            }
            // Floes hanging under the sheet: the blue ice only grows where packed ice touches it.
            if (floorMod(worldX * 7 + worldZ, 11) == 0) {
                for (std::int32_t y = 55; y <= 62; ++y) {
                    chunk.setBlockStateId(x, y, z, stateIdOf("packed_ice"));
                }
            }
            // Lone blocks with water all round them: the only support a growth finds there is the
            // one below, which is the direction the search deliberately skips.
            if (floorMod(worldX * 3 + worldZ * 5, 5) == 0) {
                for (const std::int32_t y : {44, 47, 50, 53}) {
                    chunk.setBlockStateId(x, y, z, stateIdOf("packed_ice"));
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> counted = {"packed_ice", "blue_ice", "snow_block", "snow"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 30; y <= 100; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && counted.contains(block->path())) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

struct Run {
    std::int32_t placed = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, bool surface, std::int32_t count) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = oceanChunk(chunkIndex * 2 - 9, chunkIndex * -3 + 6);
        if (surface) {
            frozenOceanDecoration::decorateSurfaceStructures(chunk, 6673LL + chunkIndex, environment);
        } else {
            frozenOceanDecoration::decorateLocalModifications(chunk, 6673LL + chunkIndex, environment);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the frozen ocean steps place what the frozen kotlin places") {
    SUBCASE("icebergs") {
        const Run result = run("minecraft:frozen_ocean", false, 64);
        CHECK(result.placed == 29627);
        CHECK(result.checksum == 8809923354053289130LL);
    }

    SUBCASE("blueIce") {
        const Run result = run("minecraft:deep_frozen_ocean", true, 8);
        CHECK(result.placed == 3569);
        CHECK(result.checksum == 3593068149887185125LL);
    }

    SUBCASE("warmIcebergs") {
        const Run result = run("minecraft:warm_ocean", false, 64);
        CHECK(result.placed == 25408);
        CHECK(result.checksum == -7987618479336093159LL);
    }

    SUBCASE("warmBlueIce") {
        const Run result = run("minecraft:warm_ocean", true, 8);
        CHECK(result.placed == 3200);
        CHECK(result.checksum == 7382046346536781290LL);
    }
}

TEST_CASE("one frozen ocean feature at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 4> kFeatures{"blue_ice", "iceberg_blue", "iceberg_packed", "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
            Chunk chunk = oceanChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(4423LL + chunkIndex * 1699LL));
            const DecorationPos origin{
                .x = chunk.chunkX() * 16 + 8, .y = 58 + chunkIndex, .z = chunk.chunkZ() * 16 + 8};
            known += frozenOceanDecoration::placeAt(chunk, random, origin, feature) ? "1" : "0";
            const auto [chunkChecksum, chunkPlaced] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            placed += chunkPlaced;
            after = random.nextInt();
        }
    }
    CHECK(known == "111111111000");
    CHECK(placed == 13988);
    CHECK(static_cast<std::int64_t>(checksum) == 8533082990884414232LL);
    CHECK(after == 661684479);
}
