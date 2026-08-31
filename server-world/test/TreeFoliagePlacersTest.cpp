#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFoliagePlacers.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using namespace basalt::server::world::terrain::decoration::tree;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Air over stone, with a log column to attach to and a pool the crown grows into, so the
/// waterlogged branch of the leaf placement runs.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
        }
    }
    for (std::int32_t y = 71; y <= 80; ++y) {
        chunk.setBlockStateId(8, y, 8, stateIdOf("oak_log"));
    }
    for (std::int32_t x = 6; x <= 10; ++x) {
        for (std::int32_t z = 9; z <= 11; ++z) {
            chunk.setBlockStateId(x, 78, z, registry::blocks::water().stateId);
        }
    }
    chunk.setBlockStateId(13, 77, 13, stateIdOf("stone"));
    return chunk;
}

struct Fold {
    std::int64_t checksum = 0;
    std::int32_t leaves = 0;
    std::int32_t waterlogged = 0;
};

[[nodiscard]] Fold fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Fold result;
    for (std::int32_t y = 69; y <= 110; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr || !block->path().ends_with("leaves")) {
                    continue;
                }
                ++result.leaves;
                const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(id);
                const auto waterlogged = properties.find("waterlogged");
                if (waterlogged != properties.end() && waterlogged->second == "true") {
                    ++result.waterlogged;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

constexpr std::array<std::int64_t, 8> kSeeds{11LL,      907LL,      31337LL,     654321LL,
                                             8675309LL, 42424242LL, 987654321LL, 5150LL};

struct Run {
    std::int32_t leaves = 0;
    std::int32_t waterlogged = 0;
    std::size_t tracked = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
    std::string shape;
};

[[nodiscard]] Run run(const FoliagePlacer& placer, bool doubleTrunk, std::int32_t radiusOffset, std::int32_t localX,
                      std::int32_t localZ) {
    constexpr std::int32_t kTreeHeight = 8;
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::size_t index = 0; index < kSeeds.size(); ++index) {
        const std::int32_t step = static_cast<std::int32_t>(index);
        Chunk chunk = groundChunk(step * 5 - 30, step * -3 + 12);
        rng::RandomSource generator = rng::RandomSource::worldgen(rng::RandomSource::legacy(kSeeds[index]));
        const FoliageAttachment attachment{.x = chunk.chunkX() * 16 + localX,
                                           .y = 79,
                                           .z = chunk.chunkZ() * 16 + localZ,
                                           .radiusOffset = radiusOffset,
                                           .doubleTrunk = doubleTrunk};
        const std::int32_t leafRadius = placer.foliageRadius(generator, kTreeHeight);
        const std::int32_t foliageHeight = placer.foliageHeight(generator, kTreeHeight);
        const std::int32_t offset = placer.offsetOf(generator);
        result.shape +=
            std::to_string(leafRadius) + ":" + std::to_string(foliageHeight) + ":" + std::to_string(offset) + ",";
        FoliagePositions positions;
        placer.createFoliage(chunk, generator, stateIdOf("oak_leaves"), attachment, foliageHeight, leafRadius, offset,
                             positions);
        result.tracked += positions.size();
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        result.leaves += folded.leaves;
        result.waterlogged += folded.waterlogged;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = generator.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the foliage placers grow what the frozen kotlin grows") {
    const IntProvider two = IntProvider::of(2);
    const IntProvider zero = IntProvider::of(0);

    SUBCASE("blob") {
        const Run result = run(FoliagePlacer::blob(two, zero, 3), false, 0, 8, 8);
        CHECK(result.leaves == 421);
        CHECK(result.waterlogged == 12);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -4836634567560293112LL);
        CHECK(result.after == -1988451487);
        CHECK(result.shape == "2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,");
    }

    SUBCASE("blobDouble") {
        const Run result = run(FoliagePlacer::blob(two, zero, 3), true, 1, 8, 8);
        CHECK(result.leaves == 1477);
        CHECK(result.waterlogged == 113);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 8833913305143083897LL);
        CHECK(result.after == -1988451487);
        CHECK(result.shape == "2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,");
    }

    SUBCASE("bush") {
        const Run result = run(FoliagePlacer::bush(two, zero, 3), false, 0, 8, 8);
        CHECK(result.leaves == 1208);
        CHECK(result.waterlogged == 68);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2947193110661350175LL);
        CHECK(result.after == -1988451487);
        CHECK(result.shape == "2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,2:3:0,");
    }

    SUBCASE("megaJungle") {
        const Run result = run(FoliagePlacer::megaJungle(two, zero, 2), false, 0, 8, 8);
        CHECK(result.leaves == 824);
        CHECK(result.waterlogged == 120);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8442176217569527611LL);
        CHECK(result.after == -1072559115);
        CHECK(result.shape == "2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,");
    }

    SUBCASE("megaJungleDouble") {
        const Run result = run(FoliagePlacer::megaJungle(two, zero, 2), true, 0, 8, 8);
        CHECK(result.leaves == 1640);
        CHECK(result.waterlogged == 120);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7604298970681674267LL);
        CHECK(result.after == 28772538);
        CHECK(result.shape == "2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,2:2:0,");
    }

    SUBCASE("fancy") {
        const Run result = run(FoliagePlacer::fancy(two, IntProvider::of(4), 0), false, 0, 8, 8);
        CHECK(result.leaves == 40);
        CHECK(result.waterlogged == 0);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1874915591775378163LL);
        CHECK(result.after == 28772538);
        CHECK(result.shape == "2:0:4,2:0:4,2:0:4,2:0:4,2:0:4,2:0:4,2:0:4,2:0:4,");
    }

    SUBCASE("acacia") {
        const Run result = run(FoliagePlacer::acacia(IntProvider::of(2), IntProvider::of(0)), false, 0, 8, 8);
        CHECK(result.leaves == 224);
        CHECK(result.waterlogged == 64);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2175536513936252603LL);
        CHECK(result.after == 28772538);
        CHECK(result.shape == "2:0:0,2:0:0,2:0:0,2:0:0,2:0:0,2:0:0,2:0:0,2:0:0,");
    }

    SUBCASE("spruce") {
        const Run result =
            run(FoliagePlacer::spruce(two, IntProvider::uniform(1, 2), IntProvider::uniform(1, 2)), false, 0, 8, 8);
        CHECK(result.leaves == 679);
        CHECK(result.waterlogged == 35);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7403678865623186433LL);
        CHECK(result.after == -699117507);
        CHECK(result.shape == "2:6:2,2:6:1,2:7:2,2:7:2,2:6:2,2:7:2,2:7:2,2:7:2,");
    }

    SUBCASE("pine") {
        const Run result = run(FoliagePlacer::pine(IntProvider::of(1), IntProvider::of(1), IntProvider::uniform(3, 4)),
                               false, 0, 8, 8);
        CHECK(result.leaves == 568);
        CHECK(result.waterlogged == 57);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1831243906510243774LL);
        CHECK(result.after == 1644477171);
        CHECK(result.shape == "1:4:1,9:3:1,9:4:1,3:4:1,5:4:1,9:4:1,6:4:1,4:4:1,");
    }

    SUBCASE("darkOak") {
        const Run result = run(FoliagePlacer::darkOak(IntProvider::of(1), zero), false, 0, 8, 8);
        CHECK(result.leaves == 544);
        CHECK(result.waterlogged == 120);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3689709033082203811LL);
        CHECK(result.after == 28772538);
        CHECK(result.shape == "1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,");
    }

    SUBCASE("darkOakDouble") {
        const Run result = run(FoliagePlacer::darkOak(IntProvider::of(1), zero), true, 0, 8, 8);
        CHECK(result.leaves == 1680);
        CHECK(result.waterlogged == 120);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -4466327923984210723LL);
        CHECK(result.after == -1072559115);
        CHECK(result.shape == "1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,1:4:0,");
    }

    SUBCASE("megaPine") {
        const Run result = run(FoliagePlacer::megaPine(zero, zero, IntProvider::uniform(13, 17)), true, 0, 8, 8);
        CHECK(result.leaves == 792);
        CHECK(result.waterlogged == 48);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8248451278770807475LL);
        CHECK(result.after == -1072559115);
        CHECK(result.shape == "0:16:0,0:17:0,0:17:0,0:15:0,0:13:0,0:15:0,0:14:0,0:17:0,");
    }

    SUBCASE("randomSpread") {
        const Run result =
            run(FoliagePlacer::randomSpread(IntProvider::of(3), IntProvider::of(0), IntProvider::of(2), 70), false, 0,
                8, 8);
        CHECK(result.leaves == 303);
        CHECK(result.waterlogged == 26);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4477555947925922322LL);
        CHECK(result.after == -1668088671);
        CHECK(result.shape == "3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,");
    }

    SUBCASE("weightedSpread") {
        const Run result = run(FoliagePlacer::weightedRandomSpread(
                                   IntProvider::of(3), IntProvider::of(0), IntProvider::of(2), 50,
                                   {{stateIdOf("azalea_leaves"), 3}, {stateIdOf("flowering_azalea_leaves"), 1}}),
                               false, 0, 8, 8);
        CHECK(result.leaves == 236);
        CHECK(result.waterlogged == 25);
        CHECK(result.tracked == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1923463467217093664LL);
        CHECK(result.after == 222335619);
        CHECK(result.shape == "3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,3:2:0,");
    }

    SUBCASE("cherry") {
        const Run result = run(FoliagePlacer::cherry(IntProvider::of(4), IntProvider::of(0), IntProvider::of(5), 0.25F,
                                                     0.5F, 0.25F, 0.25F),
                               false, 0, 8, 8);
        CHECK(result.leaves == 1071);
        CHECK(result.waterlogged == 106);
        CHECK(result.tracked == 1071);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7085895198165965840LL);
        CHECK(result.after == -94297265);
        CHECK(result.shape == "4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,");
    }

    SUBCASE("cherryEdge") {
        const Run result = run(FoliagePlacer::cherry(IntProvider::of(4), IntProvider::of(0), IntProvider::of(5), 0.25F,
                                                     0.5F, 0.25F, 0.25F),
                               false, 0, 1, 14);
        CHECK(result.leaves == 695);
        CHECK(result.waterlogged == 0);
        CHECK(result.tracked == 1103);
        CHECK(result.deferred == 408);
        CHECK(result.checksum == 5450832197447048158LL);
        CHECK(result.after == -94297265);
        CHECK(result.shape == "4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,4:5:0,");
    }
}
