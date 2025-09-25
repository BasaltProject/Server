#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GroundVegetation.hpp>

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

[[nodiscard]] Chunk groundChunk() {
    Chunk chunk{2, -3};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 70 + ((x * 5 + z * 3) % 4); ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            const std::int32_t top = 69 + ((x * 5 + z * 3) % 4);
            chunk.setBlockStateId(x, top, z, (x + z) % 3 == 0 ? stateIdOf("sand") : stateIdOf("grass_block"));
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int32_t, std::int64_t, std::int32_t>
run(const std::function<void(Chunk&, basalt::server::random::RandomSource&)>& body) {
    Chunk chunk = groundChunk();
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(31337LL));
    body(chunk, random);
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t plants = 0;
    for (std::int32_t y = 66; y <= 78; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t stateId = chunk.blockStateId(x, y, z);
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(stateId))) * 1099511628211ULL;
                if (!registry::blocks::isAir(stateId) && stateId != registry::blocks::stone().stateId &&
                    stateId != stateIdOf("grass_block") && stateId != stateIdOf("sand")) {
                    ++plants;
                }
            }
        }
    }
    return {plants, static_cast<std::int64_t>(checksum), random.nextInt()};
}

} // namespace

TEST_CASE("the ground vegetation patches place what the frozen kotlin places") {
    SUBCASE("bush") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            forestGroundDecoration::bush()(chunk, random);
        });
        CHECK(plants == 1);
        CHECK(checksum == 6234365582392740622LL);
        CHECK(after == -2089434289);
    }

    SUBCASE("forestGrass") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            forestGroundDecoration::grass()(chunk, random);
        });
        CHECK(plants == 7);
        CHECK(checksum == 6077125487700302035LL);
        CHECK(after == -390521746);
    }

    SUBCASE("savannaTall") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            savannaGroundDecoration::tallGrass()(chunk, random);
        });
        CHECK(plants == 16);
        CHECK(checksum == 2326681359912142449LL);
        CHECK(after == -1994544725);
    }

    SUBCASE("savanna") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            savannaGroundDecoration::savanna()(chunk, random);
        });
        CHECK(plants == 58);
        CHECK(checksum == -1399207164749253813LL);
        CHECK(after == -925154373);
    }

    SUBCASE("windswept") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            savannaGroundDecoration::windswept()(chunk, random);
        });
        CHECK(plants == 15);
        CHECK(checksum == 2428019299068112531LL);
        CHECK(after == -1419191976);
    }

    SUBCASE("dryDesert") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            dryGrassDecoration::desert()(chunk, random);
        });
        CHECK(plants == 7);
        CHECK(checksum == 8463449763078446664LL);
        CHECK(after == -789243648);
    }

    SUBCASE("dryBadlands") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            dryGrassDecoration::badlands()(chunk, random);
        });
        CHECK(plants == 7);
        CHECK(checksum == 8463449763078446664LL);
        CHECK(after == -789243648);
    }

    SUBCASE("desert") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            desertDecoration::decoration()(chunk, random);
        });
        CHECK(plants == 9);
        CHECK(checksum == -425545100770606844LL);
        CHECK(after == -324137968);
    }

    SUBCASE("desertAt") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            desertDecoration::placeAt(chunk, random, DecorationPos{40, 70, -40});
        });
        CHECK(plants == 2);
        CHECK(checksum == 4952213802481860123LL);
        CHECK(after == 103831914);
    }

    SUBCASE("badlands") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            badlandsDecoration::decoration()(chunk, random);
        });
        CHECK(plants == 19);
        CHECK(checksum == 4256125519418871696LL);
        CHECK(after == -1211575327);
    }

    SUBCASE("dryAt") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            dryGrassDecoration::placeAt(chunk, random, DecorationPos{40, 72, -40});
        });
        CHECK(plants == 10);
        CHECK(checksum == 3381691767254480082LL);
        CHECK(after == -1896342407);
    }

    SUBCASE("flowerAt") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            defaultVegetationDecoration::placeDefaultPatchAt(chunk, random, DecorationPos{40, 72, -40});
        });
        CHECK(plants == 6);
        CHECK(checksum == -4790080528751471068LL);
        CHECK(after == 201145552);
    }

    SUBCASE("combined") {
        const auto [plants, checksum, after] = run([](Chunk& chunk, basalt::server::random::RandomSource& random) {
            combine({forestGroundDecoration::bush(), forestGroundDecoration::grass(), dryGrassDecoration::desert()})(
                chunk, random);
        });
        CHECK(plants == 6);
        CHECK(checksum == -2385321072418939418LL);
        CHECK(after == -1786003220);
    }
}

TEST_CASE("the biome table still tells a listed biome from an unlisted one") {
    CHECK(biomeDecoration::forBiome(basalt::wire::common::Identifier::vanilla("plains")).has_value() == true);
    CHECK(biomeDecoration::forBiome(basalt::wire::common::Identifier::vanilla("nether_wastes")).has_value() == false);
}
