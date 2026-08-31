#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTrunkPlacers.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using namespace basalt::server::world::terrain::decoration::tree;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Grass over stone, plus an obstacle column and a leaf block so the free-space probes can fail.
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
    for (std::int32_t y = 71; y <= 76; ++y) {
        chunk.setBlockStateId(11, y, 11, registry::blocks::stone().stateId);
    }
    chunk.setBlockStateId(5, 72, 5, stateIdOf("oak_leaves"));
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t logs = 0;
    for (std::int32_t y = 69; y <= 110; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (treeBlockRules::isLogState(id)) {
                    ++logs;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), logs};
}

constexpr std::array<std::int64_t, 8> kSeeds{11LL,      907LL,      31337LL,     654321LL,
                                             8675309LL, 42424242LL, 987654321LL, 5150LL};

struct Run {
    std::string heights;
    std::int32_t logs = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int64_t deferredChecksum = 0;
    std::int32_t after = 0;
    std::string attached;
};

[[nodiscard]] Run run(const TrunkPlacer& placer, std::int32_t localX, std::int32_t localZ) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    std::uint64_t deferredChecksum = 14695981039346656037ULL;
    for (std::size_t index = 0; index < kSeeds.size(); ++index) {
        const std::int32_t step = static_cast<std::int32_t>(index);
        Chunk chunk = groundChunk(step * 5 - 30, step * -3 + 12);
        rng::RandomSource generator = rng::RandomSource::worldgen(rng::RandomSource::legacy(kSeeds[index]));
        const std::int32_t originX = chunk.chunkX() * 16 + localX;
        const std::int32_t originZ = chunk.chunkZ() * 16 + localZ;
        const std::int32_t treeHeight = placer.treeHeight(generator);
        result.heights += std::to_string(treeHeight) + ",";
        const std::vector<FoliageAttachment> attachments =
            placer.placeTrunk(chunk, generator, treeHeight, originX, 71, originZ, stateIdOf("oak_log"));
        for (const FoliageAttachment& attachment : attachments) {
            result.attached += std::to_string(attachment.x - originX) + ":" + std::to_string(attachment.y - 71) + ":" +
                               std::to_string(attachment.z - originZ) + ":" + std::to_string(attachment.radiusOffset) +
                               ":" + std::to_string(attachment.doubleTrunk ? 1 : 0) + " ";
        }
        result.attached += "|";
        const auto [chunkChecksum, chunkLogs] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.logs += chunkLogs;
        for (const ChunkGeneratedBlockWrite& write : chunk.takeGeneratedBlockWrites()) {
            ++result.deferred;
            for (const std::int64_t part :
                 {static_cast<std::int64_t>(write.x), static_cast<std::int64_t>(write.y),
                  static_cast<std::int64_t>(write.z), static_cast<std::int64_t>(write.stateId)}) {
                deferredChecksum = (deferredChecksum ^ static_cast<std::uint64_t>(part)) * 1099511628211ULL;
            }
        }
        result.after = generator.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    result.deferredChecksum = static_cast<std::int64_t>(deferredChecksum);
    return result;
}

} // namespace

TEST_CASE("the trunk placers build what the frozen kotlin builds") {
    SUBCASE("straight") {
        const Run result = run(TrunkPlacer::straight(4, 2, 0), 8, 8);
        CHECK(result.heights == "4,6,6,6,5,6,6,4,");
        CHECK(result.logs == 43);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2729273688631067024LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == 1644477171);
        CHECK(result.attached ==
              "0:4:0:0:0 |0:6:0:0:0 |0:6:0:0:0 |0:6:0:0:0 |0:5:0:0:0 |0:6:0:0:0 |0:6:0:0:0 |0:4:0:0:0 |");
    }

    SUBCASE("straightEdge") {
        const Run result = run(TrunkPlacer::straight(5, 2, 1), 1, 1);
        CHECK(result.heights == "6,7,8,8,7,8,8,6,");
        CHECK(result.logs == 58);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 8131705518449180375LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == 1644477171);
        CHECK(result.attached ==
              "0:6:0:0:0 |0:7:0:0:0 |0:8:0:0:0 |0:8:0:0:0 |0:7:0:0:0 |0:8:0:0:0 |0:8:0:0:0 |0:6:0:0:0 |");
    }

    SUBCASE("bending") {
        const Run result = run(TrunkPlacer::bending(1, 0, 0, 3, IntProvider::uniform(1, 2), stateIdOf("podzol")), 8, 8);
        CHECK(result.heights == "1,1,1,1,1,1,1,1,");
        CHECK(result.logs == 27);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8255199171814904508LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == 146845651);
        CHECK(result.attached == "1:1:0:0:0 2:1:0:0:0 3:1:0:0:0 |0:1:-1:0:0 0:1:-2:0:0 0:1:-3:0:0 |-1:1:0:0:0 "
                                 "-2:1:0:0:0 |0:1:-1:0:0 0:1:-2:0:0 |0:1:1:0:0 0:1:2:0:0 |-1:1:0:0:0 -2:1:0:0:0 "
                                 "-3:1:0:0:0 |-1:1:0:0:0 -2:1:0:0:0 |1:1:0:0:0 2:1:0:0:0 |");
    }

    SUBCASE("fancy") {
        const Run result = run(TrunkPlacer::fancy(3, 11, 0), 8, 8);
        CHECK(result.heights == "9,8,8,14,10,5,11,12,");
        CHECK(result.logs == 107);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5314623474414642154LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == -1227943783);
        CHECK(result.attached ==
              "0:6:0:0:0 2:5:0:0:0 |0:5:0:0:0 -1:4:0:0:0 0:3:-2:0:0 |0:5:0:0:0 -3:4:-2:0:0 0:3:-1:0:0 |0:11:0:0:0 "
              "-2:10:1:0:0 -1:9:-1:0:0 3:8:0:0:0 -2:7:3:0:0 -1:6:0:0:0 |0:7:0:0:0 -1:6:-3:0:0 1:5:1:0:0 0:4:1:0:0 "
              "|0:2:0:0:0 |0:8:0:0:0 3:7:1:0:0 1:6:1:0:0 3:5:0:0:0 1:4:-1:0:0 |0:9:0:0:0 -2:8:1:0:0 0:7:2:0:0 "
              "4:6:0:0:0 -1:5:-1:0:0 1:4:1:0:0 |");
    }

    SUBCASE("fancyEdge") {
        const Run result = run(TrunkPlacer::fancy(3, 11, 0), 2, 13);
        CHECK(result.heights == "9,8,8,14,10,5,11,12,");
        CHECK(result.logs == 105);
        CHECK(result.deferred == 2);
        CHECK(result.checksum == 3946096834178202928LL);
        CHECK(result.deferredChecksum == 6250050844250502832LL);
        CHECK(result.after == -1227943783);
        CHECK(result.attached ==
              "0:6:0:0:0 2:5:0:0:0 |0:5:0:0:0 -1:4:0:0:0 0:3:-2:0:0 |0:5:0:0:0 -3:4:-2:0:0 0:3:-1:0:0 |0:11:0:0:0 "
              "-2:10:1:0:0 -1:9:-1:0:0 3:8:0:0:0 -2:7:3:0:0 -1:6:0:0:0 |0:7:0:0:0 -1:6:-3:0:0 1:5:1:0:0 0:4:1:0:0 "
              "|0:2:0:0:0 |0:8:0:0:0 3:7:1:0:0 1:6:1:0:0 3:5:0:0:0 1:4:-1:0:0 |0:9:0:0:0 -2:8:1:0:0 0:7:2:0:0 "
              "4:6:0:0:0 -1:5:-1:0:0 1:4:1:0:0 |");
    }

    SUBCASE("forking") {
        const Run result = run(TrunkPlacer::forking(5, 2, 2), 8, 8);
        CHECK(result.heights == "7,9,7,7,6,9,8,5,");
        CHECK(result.logs == 72);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5999124108274866315LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == 383312261);
        CHECK(result.attached == "1:7:0:1:0 |0:9:-3:1:0 1:5:0:0:0 |-3:7:0:1:0 3:6:0:0:0 |0:7:-1:1:0 0:4:2:0:0 "
                                 "|0:6:2:1:0 0:5:-3:0:0 |-1:9:0:1:0 2:8:0:0:0 |-1:8:0:1:0 0:8:-3:0:0 |1:5:0:1:0 |");
    }

    SUBCASE("darkOak") {
        const Run result = run(TrunkPlacer::darkOak(6, 2, 1), 7, 7);
        CHECK(result.heights == "7,8,9,9,8,9,9,7,");
        CHECK(result.logs == 354);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2363414994762777845LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == 918253638);
        CHECK(result.attached ==
              "0:6:0:0:1 -1:6:-1:0:0 -1:6:1:0:0 0:6:-1:0:0 2:6:2:0:0 |0:7:-2:0:1 -1:7:-1:0:0 -1:7:0:0:0 2:7:0:0:0 "
              "2:7:1:0:0 |-2:8:0:0:1 -1:8:2:0:0 1:8:2:0:0 2:8:0:0:0 |0:8:0:0:1 -1:8:2:0:0 0:8:-1:0:0 1:8:-1:0:0 "
              "2:8:-1:0:0 2:8:2:0:0 |0:7:1:0:1 0:7:2:0:0 1:7:-1:0:0 2:7:-1:0:0 2:7:0:0:0 2:7:2:0:0 |0:8:0:0:1 "
              "-1:8:2:0:0 1:8:-1:0:0 1:8:2:0:0 |0:8:0:0:1 2:8:0:0:0 2:8:1:0:0 2:8:2:0:0 |0:6:0:0:1 -1:6:-1:0:0 "
              "-1:6:0:0:0 -1:6:1:0:0 1:6:-1:0:0 2:6:-1:0:0 |");
    }

    SUBCASE("giant") {
        const Run result = run(TrunkPlacer::giant(13, 2, 14), 7, 7);
        CHECK(result.heights == "21,29,18,18,17,17,22,13,");
        CHECK(result.logs == 596);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 8180639631677552633LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == 1644477171);
        CHECK(result.attached ==
              "0:21:0:0:1 |0:29:0:0:1 |0:18:0:0:1 |0:18:0:0:1 |0:17:0:0:1 |0:17:0:0:1 |0:22:0:0:1 |0:13:0:0:1 |");
    }

    SUBCASE("megaJungle") {
        const Run result = run(TrunkPlacer::megaJungle(10, 2, 19), 7, 7);
        CHECK(result.heights == "18,21,25,15,19,14,19,20,");
        CHECK(result.logs == 643);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 5899721898345236534LL);
        CHECK(result.deferredChecksum == -3750763034362895579LL);
        CHECK(result.after == 1159570266);
        CHECK(result.attached ==
              "0:18:0:0:1 2:15:5:-2:0 -1:11:3:-2:0 |0:21:0:0:1 1:19:-2:-2:0 -2:15:2:-2:0 1:13:5:-2:0 |0:25:0:0:1 "
              "0:20:-1:-2:0 -2:18:2:-2:0 5:16:2:-2:0 |0:15:0:0:1 3:13:-2:-2:0 -1:11:-1:-2:0 0:8:5:-2:0 |0:19:0:0:1 "
              "-2:15:0:-2:0 3:13:4:-2:0 5:11:1:-2:0 |0:14:0:0:1 4:9:4:-2:0 |0:19:0:0:1 2:14:5:-2:0 3:12:4:-2:0 "
              "|0:20:0:0:1 3:17:-1:-2:0 5:14:2:-2:0 |");
    }
}
