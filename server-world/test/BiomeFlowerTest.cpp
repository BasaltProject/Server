#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/BiomeFlowerFeatures.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
        }
    }
    return chunk;
}

[[nodiscard]] bool isPlant(std::int32_t id) {
    return !registry::blocks::isAir(id) && id != registry::blocks::stone().stateId && id != stateIdOf("grass_block");
}

/// The distinct species matter as much as the count: these providers pick from a noise, so a port
/// that always returned the first entry would still place the right number of blocks.
[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t flowers = 0;
    for (std::int32_t y = 69; y <= 78; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (isPlant(id)) {
                    ++flowers;
                }
            }
        }
    }
    return {checksum.value(), flowers};
}

/// The distinct species matter as much as the count: these providers pick from a noise, so a port
/// that always returned the first entry would still place the right number of blocks.
///
/// The checksum folds per chunk and then combines, which is the shape the Kotlin oracle uses: a
/// single flat fold over every block gives a different value for the same world.
[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(const std::function<void(Chunk&, basalt::server::random::RandomSource&, const DecorationPos&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t flowers = 0;
    std::set<std::int32_t> distinct;
    for (std::int32_t index = 0; index < 60; ++index) {
        Chunk chunk = groundChunk(index * 3 - 30, index * -5 + 10);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(9753ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        body(chunk, random, DecorationPos{chunk.chunkX() * 16 + 8, 71, chunk.chunkZ() * 16 + 8});
        const auto [chunkChecksum, chunkFlowers] = fold(chunk);
        checksum.feed(chunkChecksum);
        flowers += chunkFlowers;
        for (std::int32_t y = 69; y <= 78; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::int32_t id = chunk.blockStateId(x, y, z);
                    if (isPlant(id)) {
                        distinct.insert(id);
                    }
                }
            }
        }
    }
    return {flowers, distinct.size(), checksum.value()};
}

} // namespace

TEST_CASE("the biome flower patches place what the frozen kotlin places") {
    SUBCASE("flowerForest") {
        const auto [flowers, distinct, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                flowerForestDecoration::placeAt(chunk, random, origin);
            });
        CHECK(flowers == 1713);
        CHECK(distinct == 9);
        CHECK(checksum == 1197947340440142942LL);
    }

    SUBCASE("swamp") {
        // One species by construction: the swamp patch places only blue orchids.
        const auto [flowers, distinct, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                swampDecoration::placeAt(chunk, random, origin);
            });
        CHECK(flowers == 1196);
        CHECK(distinct == 1);
        CHECK(checksum == 725010371060891749LL);
    }

    SUBCASE("meadow") {
        const auto [flowers, distinct, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                meadowDecoration::placeAt(chunk, random, origin);
            });
        CHECK(flowers == 1713);
        CHECK(distinct == 7);
        CHECK(checksum == 2104764462321656270LL);
    }
}

TEST_CASE("the noise selectors pick the same species as the frozen kotlin") {
    // The selectors on their own, so the list order and the noise-to-index mapping are pinned
    // without going through a placement at all.
    std::string forest;
    std::string meadow;
    for (std::int32_t i = 0; i < 24; ++i) {
        const std::int32_t x = i * 137 - 500;
        const std::int32_t y = 64 + i;
        const std::int32_t z = i * -89 + 300;
        forest.append(std::to_string(flowerForestDecoration::flowerAt(x, y, z))).push_back(',');
        meadow.append(std::to_string(meadowDecoration::flowerStateAt(DecorationPos{x, y, z}))).push_back(',');
    }
    CHECK(forest == "2330,2330,2327,2327,2328,2326,2329,2326,2332,2326,2327,2330,2326,2328,2329,2329,2327,2331,2331,"
                    "2329,2335,2331,2328,2329,");
    CHECK(meadow == "2333,2333,2321,2321,2321,2321,2333,2333,2321,2327,2324,2321,2321,2327,2327,2321,2333,2333,2333,"
                    "2333,2333,2327,2327,2324,");
}
