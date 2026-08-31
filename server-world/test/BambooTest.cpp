#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/BambooDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <optional>
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

/// The block this terrain has at one position, shared with the reader for the neighbours.
[[nodiscard]] std::optional<std::int32_t> terrainAt(std::int32_t x, std::int32_t y, std::int32_t z) {
    if (y < -64 || y >= 100) {
        return std::nullopt;
    }
    const std::int32_t surface = 68 + floorMod(x * 3 + z * 7, 3);
    // A low ceiling over some columns, so a clump can stop at two blocks and never reach the three
    // the leafy top needs.
    if (y == surface + 3 && floorMod(x * 11 + z * 5, 6) == 2) {
        return registry::blocks::stone().stateId;
    }
    if (y > surface) {
        return registry::blocks::air().stateId;
    }
    if (y == surface) {
        // Sand and gravel hold bamboo but do not take podzol, which is the one difference between
        // the two support sets.
        switch (floorMod(x * 5 + z, 7)) {
        case 0:
            return stateIdOf("sand");
        case 1:
            return stateIdOf("gravel");
        case 2:
            return registry::blocks::stone().stateId;
        default:
            return registry::blocks::named("grass_block")->stateId;
        }
    }
    return stateIdOf("dirt");
}

[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y < 100; ++y) {
                chunk.setBlockStateId(x, y, z, *terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> counted = {"bamboo", "podzol"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 60; y < 100; ++y) {
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
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, std::int32_t index, std::int32_t count, bool outside) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    const ExternalStateReader external = outside ? terrainAt : nullptr;
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = groundChunk(chunkIndex * 3 - 4, chunkIndex * -2 + 6);
        if (index < 0) {
            bambooDecoration::decorate(chunk, 1279LL + chunkIndex, environment, external);
        } else {
            bambooDecoration::decorateFeature(index, chunk, 1279LL + chunkIndex, environment, external);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

struct Step {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Step runStep(const BiomeVegetalDecoration& step) {
    Step result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t chunkIndex = 0; chunkIndex < 12; ++chunkIndex) {
        Chunk chunk = groundChunk(chunkIndex * 2 - 1, chunkIndex + 2);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::xoroshiro(3457LL + chunkIndex));
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

} // namespace

TEST_CASE("the bamboo step places what the frozen kotlin places") {
    SUBCASE("decorateBoth") {
        const Run result = run("minecraft:jungle", -1, 24, false);
        CHECK(result.placed == 64);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 3758016832156706220LL);
    }

    SUBCASE("dense") {
        const Run result = run("minecraft:bamboo_jungle", 2, 8, false);
        CHECK(result.placed == 3566);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7903950741041420909LL);
    }

    SUBCASE("denseOutside") {
        const Run result = run("minecraft:bamboo_jungle", 2, 8, true);
        CHECK(result.placed == 3566);
        CHECK(result.deferred == 120);
        CHECK(result.checksum == -7903950741041420909LL);
    }

    SUBCASE("plains") {
        const Run result = run("minecraft:plains", -1, 24, false);
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6112272933701363318LL);
    }
}

TEST_CASE("the bamboo vegetal entries match the frozen kotlin") {
    SUBCASE("light") {
        const Step result = runStep(bambooDecoration::light());
        CHECK(result.placed == 6);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2864419494411232387LL);
        CHECK(result.after == 670117015);
    }

    SUBCASE("dense") {
        const Step result = runStep(bambooDecoration::dense());
        CHECK(result.placed == 6925);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2170328635858003083LL);
        CHECK(result.after == -524536065);
    }
}

TEST_CASE("one bamboo clump at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 4> kFeatures{"bamboo", "bamboo_light", "bamboo_vegetation", "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
            Chunk chunk = groundChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(683LL + chunkIndex * 1213LL));
            const std::int32_t worldX = chunk.chunkX() * 16 + 8;
            const std::int32_t worldZ = chunk.chunkZ() * 16 + 8;
            // The first air over the rolling surface, which is where a clump can actually root.
            const DecorationPos origin{.x = worldX, .y = 69 + floorMod(worldX * 3 + worldZ * 7, 3), .z = worldZ};
            known += bambooDecoration::placeAt(chunk, random, origin, feature) ? "1" : "0";
            const auto [chunkChecksum, chunkPlaced] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            placed += chunkPlaced;
            after = random.nextInt();
        }
    }
    CHECK(known == "111111111000");
    CHECK(placed == 68);
    CHECK(static_cast<std::int64_t>(checksum) == -7990356928351061267LL);
    CHECK(after == -775737412);
}
