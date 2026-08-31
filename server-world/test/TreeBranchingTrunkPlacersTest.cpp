#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/MangroveRootPlacer.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeTrunkPlacers.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using namespace basalt::server::world::terrain::decoration::tree;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Grass over stone, with an obstacle column so the trunk probes can fail.
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
    for (std::int32_t y = 71; y <= 78; ++y) {
        chunk.setBlockStateId(11, y, 11, registry::blocks::stone().stateId);
    }
    return chunk;
}

/// A mangrove swamp: mud under shallow water, which is what the roots grow through.
[[nodiscard]] Chunk swampChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 66; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = 66; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("mud"));
            }
            for (std::int32_t y = 71; y <= 73; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
        }
    }
    for (std::int32_t y = 66; y <= 73; ++y) {
        chunk.setBlockStateId(12, y, 12, registry::blocks::stone().stateId);
    }
    return chunk;
}

struct Fold {
    std::int64_t checksum = 0;
    std::int32_t logs = 0;
    std::int32_t roots = 0;
};

[[nodiscard]] Fold fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Fold result;
    for (std::int32_t y = 60; y <= 110; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (treeBlockRules::isLogState(id)) {
                    ++result.logs;
                }
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr) {
                    continue;
                }
                const std::string path = block->path();
                if (path == "mangrove_roots" || path == "muddy_mangrove_roots" || path == "moss_carpet") {
                    ++result.roots;
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
    std::int32_t logs = 0;
    std::int32_t roots = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
    std::string detail;
};

using Body = std::function<std::string(Chunk&, rng::RandomSource&, std::int32_t, std::int32_t)>;

[[nodiscard]] Run run(std::int32_t localX, std::int32_t localZ, bool swamp, const Body& body) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::size_t index = 0; index < kSeeds.size(); ++index) {
        const std::int32_t step = static_cast<std::int32_t>(index);
        Chunk chunk = swamp ? swampChunk(step * 5 - 30, step * -3 + 12) : groundChunk(step * 5 - 30, step * -3 + 12);
        rng::RandomSource generator = rng::RandomSource::worldgen(rng::RandomSource::legacy(kSeeds[index]));
        result.detail += body(chunk, generator, chunk.chunkX() * 16 + localX, chunk.chunkZ() * 16 + localZ) + "|";
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        result.logs += folded.logs;
        result.roots += folded.roots;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = generator.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

[[nodiscard]] Body trunkBody(const TrunkPlacer& placer, std::int32_t originY) {
    return [&placer, originY](Chunk& chunk, rng::RandomSource& generator, std::int32_t originX, std::int32_t originZ) {
        const std::int32_t treeHeight = placer.treeHeight(generator);
        const std::vector<FoliageAttachment> attachments =
            placer.placeTrunk(chunk, generator, treeHeight, originX, originY, originZ, stateIdOf("mangrove_log"));
        std::string detail = std::to_string(treeHeight) + ";";
        for (const FoliageAttachment& a : attachments) {
            detail += std::to_string(a.x - originX) + ":" + std::to_string(a.y - originY) + ":" +
                      std::to_string(a.z - originZ) + ":" + std::to_string(a.radiusOffset) + " ";
        }
        return detail;
    };
}

} // namespace

TEST_CASE("the mangrove and cherry trunks match the frozen kotlin") {
    // parity: the real mangrove and cherry trunk placer arguments.
    const TrunkPlacer mangrove =
        TrunkPlacer::upwardsBranching(2, 1, 4, IntProvider::uniform(1, 4), 0.5F, IntProvider::uniform(0, 1));
    const TrunkPlacer cherry = TrunkPlacer::cherry(7, 1, 0, IntProvider::uniform(1, 3), IntProvider::uniform(2, 4), -4,
                                                   -3, IntProvider::uniform(-1, 0));

    SUBCASE("mangroveTrunk") {
        const Run result = run(8, 8, true, trunkBody(mangrove, 74));
        CHECK(result.logs == 61);
        CHECK(result.roots == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -6775629778876321358LL);
        CHECK(result.after == 1159570266);
        CHECK(result.detail ==
              "6;1:3:0:0 1:4:0:0 1:2:0:0 0:6:0:0 |7;0:2:1:0 0:3:2:0 0:4:2:0 0:2:2:0 0:5:-1:0 0:6:-2:0 0:7:-2:0 "
              "0:5:-2:0 0:6:-1:0 0:7:-2:0 0:8:-2:0 0:6:-2:0 0:7:0:0 |5;1:3:0:0 2:4:0:0 2:5:0:0 2:3:0:0 1:4:0:0 2:5:0:0 "
              "2:6:0:0 2:4:0:0 0:5:0:0 |5;-1:1:0:0 -1:2:0:0 -1:0:0:0 0:2:1:0 0:3:2:0 0:4:3:0 0:5:3:0 0:3:3:0 0:5:0:0 "
              "|6;0:3:-1:0 0:4:-2:0 0:5:-2:0 0:3:-2:0 0:6:0:0 |4;-1:2:0:0 -2:3:0:0 -3:4:0:0 -3:5:0:0 -3:3:0:0 0:4:0:0 "
              "|4;0:2:-1:0 0:3:-1:0 0:1:-1:0 0:4:0:0 |2;-1:1:0:0 -1:2:0:0 -1:0:0:0 0:2:0:0 |");
    }

    SUBCASE("mangroveTrunkGround") {
        const Run result = run(8, 8, false, trunkBody(mangrove, 71));
        CHECK(result.logs == 61);
        CHECK(result.roots == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2584464326528160722LL);
        CHECK(result.after == 1159570266);
        CHECK(result.detail ==
              "6;1:3:0:0 1:4:0:0 1:2:0:0 0:6:0:0 |7;0:2:1:0 0:3:2:0 0:4:2:0 0:2:2:0 0:5:-1:0 0:6:-2:0 0:7:-2:0 "
              "0:5:-2:0 0:6:-1:0 0:7:-2:0 0:8:-2:0 0:6:-2:0 0:7:0:0 |5;1:3:0:0 2:4:0:0 2:5:0:0 2:3:0:0 1:4:0:0 2:5:0:0 "
              "2:6:0:0 2:4:0:0 0:5:0:0 |5;-1:1:0:0 -1:2:0:0 -1:0:0:0 0:2:1:0 0:3:2:0 0:4:3:0 0:5:3:0 0:3:3:0 0:5:0:0 "
              "|6;0:3:-1:0 0:4:-2:0 0:5:-2:0 0:3:-2:0 0:6:0:0 |4;-1:2:0:0 -2:3:0:0 -3:4:0:0 -3:5:0:0 -3:3:0:0 0:4:0:0 "
              "|4;0:2:-1:0 0:3:-1:0 0:1:-1:0 0:4:0:0 |2;-1:1:0:0 -1:2:0:0 -1:0:0:0 0:2:0:0 |");
    }

    SUBCASE("cherryTrunk") {
        const Run result = run(8, 8, false, trunkBody(cherry, 71));
        CHECK(result.logs == 108);
        CHECK(result.roots == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -7967514552460191236LL);
        CHECK(result.after == -999051014);
        CHECK(result.detail == "8;4:7:0:0 |8;2:7:0:0 |7;4:6:0:0 |7;0:7:0:0 0:6:4:0 0:6:-4:0 |8;0:7:-4:0 0:7:4:0 "
                               "|7;3:7:0:0 |7;0:7:-4:0 |7;0:7:0:0 0:7:-3:0 0:6:3:0 |");
    }

    SUBCASE("cherryTrunkEdge") {
        const Run result = run(2, 13, false, trunkBody(cherry, 71));
        CHECK(result.logs == 97);
        CHECK(result.roots == 0);
        CHECK(result.deferred == 11);
        CHECK(result.checksum == 464578910494357924LL);
        CHECK(result.after == -999051014);
        CHECK(result.detail == "8;4:7:0:0 |8;2:7:0:0 |7;4:6:0:0 |7;0:7:0:0 0:6:4:0 0:6:-4:0 |8;0:7:-4:0 0:7:4:0 "
                               "|7;3:7:0:0 |7;0:7:-4:0 |7;0:7:0:0 0:7:-3:0 0:6:3:0 |");
    }
}

TEST_CASE("the mangrove roots match the frozen kotlin") {
    const MangroveRootPlacer roots{IntProvider::uniform(1, 3)};
    const Body body = [&roots](Chunk& chunk, rng::RandomSource& generator, std::int32_t originX, std::int32_t originZ) {
        const std::int32_t trunkOriginY = roots.trunkOriginY(generator, 71);
        const bool placed = roots.placeRoots(chunk, generator, originX, 71, originZ, trunkOriginY);
        return std::to_string(trunkOriginY) + ":" + (placed ? "true" : "false");
    };

    SUBCASE("mangroveRoots") {
        const Run result = run(8, 8, true, body);
        CHECK(result.logs == 0);
        CHECK(result.roots == 329);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 397683177182163859LL);
        CHECK(result.after == 332870239);
        CHECK(result.detail == "72:true|74:true|74:true|74:true|73:true|74:true|74:false|72:true|");
    }

    SUBCASE("mangroveRootsBlocked") {
        const Run result = run(12, 12, true, body);
        CHECK(result.logs == 0);
        CHECK(result.roots == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -6848695760125221595LL);
        CHECK(result.after == -1072559115);
        CHECK(result.detail == "72:false|74:false|74:false|74:false|73:false|74:false|74:false|72:false|");
    }
}
