#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FlowerFeatures.hpp>

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

/// Grass with a leaf canopy over half of it: the pale garden patch uses the no-leaves heightmap, so
/// a chunk without leaves would never tell that heightmap apart from the ordinary one.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
            if (x < 8) {
                chunk.setBlockStateId(x, 76, z, stateIdOf("oak_leaves"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t eyeblossom = 0;
    std::int32_t petals = 0;
    for (std::int32_t y = 69; y <= 80; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == stateIdOf("closed_eyeblossom")) {
                    ++eyeblossom;
                }
                if (registry::blockStateFamilies::isPinkPetalsState(id)) {
                    ++petals;
                }
            }
        }
    }
    return {checksum.value(), eyeblossom, petals};
}

[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(const std::function<void(Chunk&, basalt::server::random::RandomSource&, const DecorationPos&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t eyeblossom = 0;
    std::int32_t petals = 0;
    std::size_t ticks = 0;
    for (std::int32_t index = 0; index < 60; ++index) {
        Chunk chunk = groundChunk(index * 3 - 30, index * -5 + 10);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(4321ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        body(chunk, random, DecorationPos{chunk.chunkX() * 16 + 8, 71, chunk.chunkZ() * 16 + 8});
        const auto [chunkChecksum, chunkEyes, chunkPetals] = fold(chunk);
        checksum.feed(chunkChecksum);
        eyeblossom += chunkEyes;
        petals += chunkPetals;
        ticks += chunk.scheduledBlockTicks().size();
    }
    return {eyeblossom, petals, ticks, checksum.value()};
}

} // namespace

TEST_CASE("the pale garden and cherry petal features match the frozen kotlin") {
    SUBCASE("paleFlowerPatch") {
        // Every eyeblossom schedules its own tick, so the two counts have to move together.
        const auto [eyeblossom, petals, ticks, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                (void)origin;
                paleGardenDecoration::flowerPatch()(chunk, random);
            });
        CHECK(eyeblossom == 114);
        CHECK(petals == 0);
        CHECK(ticks == 114);
        CHECK(checksum == -6631655323367291439LL);
    }

    SUBCASE("paleRareFlower") {
        // Every eyeblossom schedules its own tick, so the two counts have to move together.
        const auto [eyeblossom, petals, ticks, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                (void)origin;
                paleGardenDecoration::rareFlower()(chunk, random);
            });
        CHECK(eyeblossom == 2);
        CHECK(petals == 0);
        CHECK(ticks == 2);
        CHECK(checksum == -573311641727884055LL);
    }

    SUBCASE("paleAt") {
        // Every eyeblossom schedules its own tick, so the two counts have to move together.
        const auto [eyeblossom, petals, ticks, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                (void)origin;
                paleGardenDecoration::placeAt(chunk, random, origin);
            });
        CHECK(eyeblossom == 1330);
        CHECK(petals == 0);
        CHECK(ticks == 1330);
        CHECK(checksum == -43465349220930415LL);
    }

    SUBCASE("paleSingleAt") {
        // Every eyeblossom schedules its own tick, so the two counts have to move together.
        const auto [eyeblossom, petals, ticks, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                (void)origin;
                paleGardenDecoration::placeSingleAt(chunk, random, origin);
            });
        CHECK(eyeblossom == 60);
        CHECK(petals == 0);
        CHECK(ticks == 60);
        CHECK(checksum == -934020570696224583LL);
    }

    SUBCASE("cherryFlowers") {
        const auto [eyeblossom, petals, ticks, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                (void)origin;
                cherryGroveDecoration::flowersOnly()(chunk, random);
            });
        CHECK(eyeblossom == 0);
        CHECK(petals == 4642);
        CHECK(ticks == 0);
        CHECK(checksum == 8611934684162717064LL);
    }

    SUBCASE("cherryAt") {
        const auto [eyeblossom, petals, ticks, checksum] =
            run([](Chunk& chunk, basalt::server::random::RandomSource& random, const DecorationPos& origin) {
                (void)origin;
                cherryGroveDecoration::placeAt(chunk, random, origin);
            });
        CHECK(eyeblossom == 0);
        CHECK(petals == 1672);
        CHECK(ticks == 0);
        CHECK(checksum == -2759389312408167190LL);
    }
}

TEST_CASE("the petal patch produces all sixteen packed states") {
    // Read off what actually lands rather than from the packing helper, so the amount and facing
    // arithmetic is pinned by observed blocks.
    std::set<std::int32_t> seen;
    for (std::int32_t index = 0; index < 60; ++index) {
        Chunk chunk = groundChunk(index * 3 - 30, index * -5 + 10);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(4321ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        cherryGroveDecoration::placeAt(chunk, random,
                                       DecorationPos{chunk.chunkX() * 16 + 8, 71, chunk.chunkZ() * 16 + 8});
        for (std::int32_t y = 69; y <= 80; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::int32_t id = chunk.blockStateId(x, y, z);
                    if (registry::blockStateFamilies::isPinkPetalsState(id)) {
                        seen.insert(id);
                    }
                }
            }
        }
    }
    std::string rendered;
    for (const std::int32_t id : seen) {
        if (!rendered.empty()) {
            rendered.push_back(',');
        }
        rendered.append(std::to_string(id));
    }
    CHECK(rendered ==
          "30307,30308,30309,30310,30311,30312,30313,30314,30315,30316,30317,30318,30319,30320,30321,30322");
}
