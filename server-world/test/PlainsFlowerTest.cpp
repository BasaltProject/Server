#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/PlainsFlowerFeatures.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <utility>

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

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t flowers = 0;
    std::int32_t wild = 0;
    for (std::int32_t y = 69; y <= 78; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (registry::blockStateFamilies::isWildflowersState(id)) {
                    ++wild;
                } else if (isPlant(id)) {
                    ++flowers;
                }
            }
        }
    }
    return {checksum.value(), flowers, wild};
}

/// The checksum folds per chunk and then combines, matching the Kotlin oracle's shape: a single flat
/// fold over every block gives a different value for the same world.
[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(const std::function<void(Chunk&, basalt::server::random::RandomSource&, const DecorationPos&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t flowers = 0;
    std::int32_t wild = 0;
    std::set<std::int32_t> distinct;
    for (std::int32_t index = 0; index < 60; ++index) {
        Chunk chunk = groundChunk(index * 3 - 30, index * -5 + 10);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(1357ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        body(chunk, random, DecorationPos{chunk.chunkX() * 16 + 8, 71, chunk.chunkZ() * 16 + 8});
        const auto [chunkChecksum, chunkFlowers, chunkWild] = fold(chunk);
        checksum.feed(chunkChecksum);
        flowers += chunkFlowers;
        wild += chunkWild;
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
    return {flowers, wild, distinct.size(), checksum.value()};
}

} // namespace

TEST_CASE("the plains and wildflower patches place what the frozen kotlin places") {
    SUBCASE("plainsFlowers") {
        const auto [flowers, wild, distinct, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                plainsDecoration::placeFlowerPlainAt(chunk, random, origin.x, origin.y, origin.z);
            });
        CHECK(flowers == 1132);
        CHECK(wild == 0);
        CHECK(distinct == 5);
        CHECK(checksum == -7454913585254825378LL);
    }

    SUBCASE("wildflowersBirch") {
        // All sixteen packed states appear, which is what pins the amount and facing
        // arithmetic through the placement rather than through the helper.
        const auto [flowers, wild, distinct, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                forestFloorDecoration::placeWildflowersAt(chunk, random, origin, 64);
            });
        CHECK(flowers == 0);
        CHECK(wild == 1141);
        CHECK(distinct == 16);
        CHECK(checksum == -3722645096143717799LL);
    }

    SUBCASE("wildflowersMeadow") {
        // All sixteen packed states appear, which is what pins the amount and facing
        // arithmetic through the placement rather than through the helper.
        const auto [flowers, wild, distinct, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                forestFloorDecoration::placeWildflowersAt(chunk, random, origin, 8);
            });
        CHECK(flowers == 0);
        CHECK(wild == 146);
        CHECK(distinct == 16);
        CHECK(checksum == -5501126426333239231LL);
    }
}

TEST_CASE("the plains selector picks the same flowers as the frozen kotlin") {
    // It reads the noise and then draws, so the sequence pins both the threshold and the draws. The
    // trailing value proves it consumed exactly as many as vanilla's.
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(555LL));
    std::string picks;
    for (std::int32_t i = 0; i < 40; ++i) {
        const std::int32_t x = i * 211 - 4000;
        const std::int32_t z = i * -173 + 3000;
        picks.append(std::to_string(plainsDecoration::flowerStateId(random, DecorationPos{x, 71, z}))).push_back(',');
    }
    CHECK(picks ==
          "3813,3824,3825,3813,3813,3813,3813,3813,3816,3825,3824,3813,3824,3813,3813,3813,3813,3813,3813,3813,"
          "3824,3816,3825,3816,3813,3813,3813,3813,3819,3824,3813,3813,3813,3816,3813,3813,3813,3813,3813,3813,");
    CHECK(random.nextInt() == 1556930442);
}

TEST_CASE("the flower bed packing matches the frozen kotlin") {
    std::string beds;
    for (std::int32_t choice = 0; choice < 16; ++choice) {
        beds.append(std::to_string(forestFloorDecoration::flowerBedStateId(stateIdOf("wildflowers"), choice)))
            .push_back(',');
    }
    CHECK(beds == "33680,33692,33684,33688,33681,33693,33685,33689,33682,33694,33686,33690,33683,33695,33687,33691,");
}
