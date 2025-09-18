#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FlowerVegetation.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 71; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            // Podzol in a stripe so the jungle patch's not-on-podzol filter really rejects.
            chunk.setBlockStateId(x, 70, z, z % 3 == 0 ? stateIdOf("podzol") : stateIdOf("grass_block"));
        }
    }
    return chunk;
}

/// Over several chunks rather than one: these patches sit behind rarity filters of up to 1/512, so a
/// single chunk mostly measures the filter rejecting and never reaches the placement itself.
[[nodiscard]] std::tuple<std::int32_t, std::int64_t, std::int32_t>
run(const std::function<void(Chunk&, basalt::server::random::RandomSource&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t plants = 0;
    // Wrapping, the way the Kotlin Int this is compared against wraps: signed overflow is undefined.
    std::uint32_t after = 0;
    for (std::int32_t index = 0; index < 40; ++index) {
        Chunk chunk = groundChunk(-7 + index * 3, 4 - index * 5);
        // Sequential legacy seeds leave the first draw correlated, and a rarity filter reads exactly
        // that draw: spaced seeds are what let these patches ever pass their filter.
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(24680ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        body(chunk, random);
        for (std::int32_t y = 68; y <= 80; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::int32_t id = chunk.blockStateId(x, y, z);
                    checksum =
                        (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                    if (!registry::blocks::isAir(id) && id != registry::blocks::stone().stateId &&
                        id != stateIdOf("grass_block") && id != stateIdOf("podzol")) {
                        ++plants;
                    }
                }
            }
        }
        after = after * 31U + static_cast<std::uint32_t>(random.nextInt());
    }
    return {plants, static_cast<std::int64_t>(checksum), static_cast<std::int32_t>(after)};
}

} // namespace

TEST_CASE("the flower, mushroom and jungle patches place what the frozen kotlin places") {
    SUBCASE("flowerNormal") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            forestFlowerDecoration::normal()(chunk, random);
        });
        CHECK(plants == 44);
        CHECK(checksum == 5958697904001729611LL);
        CHECK(after == 1006776013);
    }

    SUBCASE("flowerForest") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            forestFlowerDecoration::flowerForest()(chunk, random);
        });
        CHECK(plants == 156);
        CHECK(checksum == -590134980737649685LL);
        CHECK(after == -1426122566);
    }

    SUBCASE("flowerAt") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            forestFlowerDecoration::placeAt(chunk, random, DecorationPos{-108, 71, 68});
        });
        CHECK(plants == 36);
        CHECK(checksum == -2145021934335469461LL);
        CHECK(after == 222620013);
    }

    SUBCASE("mushroomNormal") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            mushroomDecoration::normal()(chunk, random);
        });
        CHECK(plants == 0);
        CHECK(checksum == -7381869660242326747LL);
        CHECK(after == -1706332628);
    }

    SUBCASE("mushroomTaiga") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            mushroomDecoration::taiga()(chunk, random);
        });
        CHECK(plants == 116);
        CHECK(checksum == 965689513208632485LL);
        CHECK(after == 1485954835);
    }

    SUBCASE("mushroomOldGrowth") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            mushroomDecoration::oldGrowth()(chunk, random);
        });
        CHECK(plants == 527);
        CHECK(checksum == 1651717318914920215LL);
        CHECK(after == 1633551061);
    }

    SUBCASE("mushroomSwamp") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            mushroomDecoration::swamp()(chunk, random);
        });
        CHECK(plants == 1135);
        CHECK(checksum == -1899248003956468723LL);
        CHECK(after == -722673447);
    }

    SUBCASE("jungleGrass") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            jungleGrassDecoration::decoration()(chunk, random);
        });
        CHECK(plants == 2754);
        CHECK(checksum == 2829441298885743888LL);
        CHECK(after == 494956733);
    }

    SUBCASE("jungleAt") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            jungleGrassDecoration::placePatchAt(chunk, random, DecorationPos{-108, 71, 68});
        });
        CHECK(plants == 7);
        CHECK(checksum == -338084318538720604LL);
        CHECK(after == -1538952572);
    }
}
