#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/BiomeFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/FlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/PlainsFlowerFeatures.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ, std::string_view surface) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf(surface));
        }
    }
    return chunk;
}

[[nodiscard]] DecorationPos at(const Chunk& chunk) {
    return DecorationPos{.x = chunk.chunkX() * 16 + 8, .y = 71, .z = chunk.chunkZ() * 16 + 8};
}

[[nodiscard]] std::int64_t checksumOf(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t y = 60; y <= 140; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(chunk.blockStateId(x, y, z)))) *
                    1099511628211ULL;
            }
        }
    }
    return static_cast<std::int64_t>(checksum);
}

/// Everything the terrain builder did not put there.
[[nodiscard]] std::int32_t plants(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ground = {"grass_block", "podzol"};
    std::int32_t count = 0;
    for (std::int32_t y = 60; y <= 140; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                if (id == registry::blocks::air().stateId || id == registry::blocks::stone().stateId) {
                    continue;
                }
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr || !ground.contains(block->path())) {
                    ++count;
                }
            }
        }
    }
    return count;
}

struct Run {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(std::string_view surface, const std::function<void(Chunk&, rng::RandomSource&)>& body) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 8; ++index) {
        Chunk chunk = groundChunk(index * 5 - 30, index * -3 + 12, surface);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(7000LL + index * 977LL));
        body(chunk, random);
        checksum = (checksum ^ static_cast<std::uint64_t>(checksumOf(chunk))) * 1099511628211ULL;
        result.placed += plants(chunk);
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the biome flora places what the frozen kotlin places") {
    SUBCASE("cherryGrove") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            cherryGroveDecoration::decorate(chunk, random);
        });
        CHECK(result.placed == 9674);
        CHECK(result.deferred == 3667);
        CHECK(result.checksum == -4855727092449001036LL);
        CHECK(result.after == -953260349);
    }

    SUBCASE("cherryPetals") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            cherryGroveDecoration::flowersOnly()(chunk, random);
        });
        CHECK(result.placed == 1113);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7553418535622776388LL);
        CHECK(result.after == 1651480646);
    }

    SUBCASE("cherryPetalsAt") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            cherryGroveDecoration::placeAt(chunk, random, at(chunk));
        });
        CHECK(result.placed == 250);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2455571490673551476LL);
        CHECK(result.after == -759981502);
    }

    SUBCASE("meadow") {
        const Run result = run(
            "grass_block", [](Chunk& chunk, rng::RandomSource& random) { meadowDecoration::decorate(chunk, random); });
        CHECK(result.placed == 307);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2415157409987665524LL);
        CHECK(result.after == 37078178);
    }

    SUBCASE("meadowWithoutTrees") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            meadowDecoration::withoutTrees()(chunk, random);
        });
        CHECK(result.placed == 307);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8945786533924849926LL);
        CHECK(result.after == 2118066002);
    }

    SUBCASE("meadowGrass") {
        const Run result = run(
            "grass_block", [](Chunk& chunk, rng::RandomSource& random) { meadowDecoration::grass()(chunk, random); });
        CHECK(result.placed == 222);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7504538955792236109LL);
        CHECK(result.after == -233599858);
    }

    SUBCASE("meadowFlowersAt") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            meadowDecoration::placeAt(chunk, random, at(chunk));
        });
        CHECK(result.placed == 245);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 650297213759933874LL);
        CHECK(result.after == -1965818250);
    }

    SUBCASE("flowerForest") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            flowerForestDecoration::decorate(chunk, random);
        });
        CHECK(result.placed == 2990);
        CHECK(result.deferred == 497);
        CHECK(result.checksum == 6571297958160010689LL);
        CHECK(result.after == 1134608129);
    }

    SUBCASE("flowerForestFlowers") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            flowerForestDecoration::flowersOnly()(chunk, random);
        });
        CHECK(result.placed == 251);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3361674207766894234LL);
        CHECK(result.after == -1390898867);
    }

    SUBCASE("birchWildflowers") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            forestFloorDecoration::birchWildflowers()(chunk, random);
        });
        CHECK(result.placed == 173);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5727417360229542913LL);
        CHECK(result.after == -1872713568);
    }

    SUBCASE("meadowWildflowers") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            forestFloorDecoration::meadowWildflowers()(chunk, random);
        });
        CHECK(result.placed == 167);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3798254689443214587LL);
        CHECK(result.after == -1866735801);
    }

    SUBCASE("leafLitter") {
        const Run result = run("grass_block", [](Chunk& chunk, rng::RandomSource& random) {
            forestFloorDecoration::leafLitter()(chunk, random);
        });
        CHECK(result.placed == 103);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1771595825668374332LL);
        CHECK(result.after == -1722221598);
    }

    SUBCASE("leafLitterWrongGround") {
        const Run result = run("podzol", [](Chunk& chunk, rng::RandomSource& random) {
            forestFloorDecoration::leafLitter()(chunk, random);
        });
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4818777284810462349LL);
        CHECK(result.after == 1078215224);
    }
}

TEST_CASE("the noise flower providers match the frozen kotlin position by position") {
    std::string flowerForest;
    std::string meadow;
    for (std::int32_t i = 0; i < 40; ++i) {
        const std::int32_t x = i * 37 - 400;
        const std::int32_t z = i * -53 + 250;
        flowerForest += std::to_string(flowerForestDecoration::flowerAt(x, 71, z)) + ",";
        meadow += std::to_string(meadowDecoration::flowerStateAt(DecorationPos{.x = x, .y = 71, .z = z})) + ",";
    }
    CHECK(flowerForest ==
          "2329,2331,2332,2329,2329,2329,2327,2328,2329,2329,2330,2330,2330,2330,2328,2330,2328,2329,2331,2327,2328,"
          "2330,2331,2329,2324,2324,2329,2331,2329,2329,2327,2324,2326,2331,2328,2330,2331,2327,2329,2331,");
    CHECK(meadow ==
          "2333,2333,2321,2333,2333,2333,2321,2321,2333,2333,2321,2332,2332,2332,2327,2332,2332,2332,2332,2332,2326,"
          "2332,2333,2333,2333,2333,2321,2321,2333,2333,2321,2321,2321,2332,2321,2321,2332,2333,2332,2333,");
}
