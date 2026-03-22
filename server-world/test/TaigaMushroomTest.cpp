#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/MushroomFieldsDecoration.hpp>
#include <basalt/server/world/terrain/decoration/TaigaGroundDecoration.hpp>

#include <doctest.h>

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

/// Grass over dirt with a rolling surface and a mycelium band, so both ground rules apply.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            const std::int32_t surface = 66 + floorMod(worldX * 3 + worldZ * 5, 4);
            for (std::int32_t y = 60; y < surface; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("dirt"));
            }
            const std::int32_t top = floorMod(worldX + worldZ, 9) == 0
                                         ? stateIdOf("mycelium")
                                         : registry::blocks::named("grass_block")->stateId;
            chunk.setBlockStateId(x, surface, z, top);
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ignored = {"dirt", "grass_block", "mycelium", "air"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 60; y <= 95; ++y) {
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

struct Run {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(const BiomeVegetalDecoration& step, std::int32_t count) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = groundChunk(chunkIndex * 2 - 3, chunkIndex * -3 + 8);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(9127LL + chunkIndex));
        step(chunk, random);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] Run runStep(std::string_view biome) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    for (std::int32_t chunkIndex = 0; chunkIndex < 6; ++chunkIndex) {
        Chunk chunk = groundChunk(chunkIndex * 2 - 3, chunkIndex * -3 + 8);
        mushroomFieldsDecoration::decorate(chunk, 5431LL + chunkIndex, environment);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the taiga and mushroom field vegetal entries match the frozen kotlin") {
    SUBCASE("largeFern") {
        const Run result = run(taigaGroundDecoration::largeFern(), 24);
        CHECK(result.placed == 56);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7266825529492433117LL);
        CHECK(result.after == -1028147194);
    }

    SUBCASE("normalGrass") {
        const Run result = run(taigaGroundDecoration::normal(), 6);
        CHECK(result.placed == 23);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2424720565467432304LL);
        CHECK(result.after == 1150172841);
    }

    SUBCASE("oldGrowth") {
        const Run result = run(taigaGroundDecoration::oldGrowth(), 6);
        CHECK(result.placed == 156);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7298676477525820248LL);
        CHECK(result.after == -1039769469);
    }

    SUBCASE("deadBush") {
        const Run result = run(taigaGroundDecoration::oldGrowthDeadBush(), 6);
        CHECK(result.placed == 2);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1413164977039937323LL);
        CHECK(result.after == -1118636042);
    }

    SUBCASE("commonBerry") {
        const Run result = run(taigaGroundDecoration::commonBerry(), 128);
        CHECK(result.placed == 15);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1934192332380869058LL);
        CHECK(result.after == 609261023);
    }

    SUBCASE("rareBerry") {
        const Run result = run(taigaGroundDecoration::rareBerry(), 512);
        CHECK(result.placed == 9);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2059856785571888128LL);
        CHECK(result.after == 773648398);
    }

    SUBCASE("mushroomHuge") {
        const Run result = run(mushroomFieldsDecoration::hugeOnly(), 6);
        CHECK(result.placed == 263);
        CHECK(result.deferred == 26);
        CHECK(result.checksum == 8181045599831427112LL);
        CHECK(result.after == 1292384349);
    }

    SUBCASE("mushroomVegetal") {
        const Run result = run(mushroomFieldsDecoration::vegetalDecoration(), 6);
        CHECK(result.placed == 276);
        CHECK(result.deferred == 26);
        CHECK(result.checksum == 9135215667896438164LL);
        CHECK(result.after == -1219156631);
    }
}

TEST_CASE("the mushroom island step matches the frozen kotlin") {
    SUBCASE("mushroomStep") {
        const Run result = runStep("minecraft:mushroom_fields");
        CHECK(result.placed == 295);
        CHECK(result.checksum == 7702891816398270437LL);
    }

    SUBCASE("mushroomPlains") {
        const Run result = runStep("minecraft:plains");
        CHECK(result.placed == 0);
        CHECK(result.checksum == 2289059197692114875LL);
    }
}
