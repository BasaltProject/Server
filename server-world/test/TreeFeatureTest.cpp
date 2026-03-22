#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <set>
#include <string>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using namespace basalt::server::world::terrain::decoration::tree;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

enum class Ground : std::uint8_t { Ground, Swamp, Creaking };

/// Grass over stone, with one obstacle column so the free-height clip can bite.
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
        chunk.setBlockStateId(14, y, 2, registry::blocks::stone().stateId);
    }
    return chunk;
}

/// The same ground with a neighbouring trunk already standing: a creaking heart needs a log with
/// logs on all six sides, which one tree on its own can never produce.
[[nodiscard]] Chunk creakingChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk = groundChunk(chunkX, chunkZ);
    for (std::int32_t x = 6; x <= 7; ++x) {
        for (std::int32_t z = 7; z <= 8; ++z) {
            for (std::int32_t y = 71; y <= 80; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("pale_oak_log"));
            }
        }
    }
    return chunk;
}

/// Mud under shallow water, which is the only ground a mangrove will take.
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
            for (std::int32_t y = 71; y <= 72; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

const std::set<std::string, std::less<>>& decorationPaths() {
    static const std::set<std::string, std::less<>> paths = {"vine",
                                                             "cocoa",
                                                             "podzol",
                                                             "bee_nest",
                                                             "leaf_litter",
                                                             "pale_hanging_moss",
                                                             "creaking_heart",
                                                             "mangrove_propagule",
                                                             "pale_moss_block",
                                                             "pale_moss_carpet"};
    return paths;
}

struct Counts {
    std::int32_t logs = 0;
    std::int32_t leaves = 0;
    std::int32_t decorations = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Counts fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Counts result;
    for (std::int32_t y = 60; y <= 140; ++y) {
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
                if (path.ends_with("leaves")) {
                    ++result.leaves;
                }
                if (decorationPaths().contains(path)) {
                    ++result.decorations;
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
    std::string placed;
    std::int32_t logs = 0;
    std::int32_t leaves = 0;
    std::int32_t decorations = 0;
    std::size_t deferred = 0;
    std::size_t entities = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(const TreeConfiguration& config, std::int32_t originY, Ground ground, std::int32_t localX,
                      std::int32_t localZ) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::size_t index = 0; index < kSeeds.size(); ++index) {
        const std::int32_t step = static_cast<std::int32_t>(index);
        const std::int32_t chunkX = step * 5 - 30;
        const std::int32_t chunkZ = step * -3 + 12;
        Chunk chunk = ground == Ground::Swamp      ? swampChunk(chunkX, chunkZ)
                      : ground == Ground::Creaking ? creakingChunk(chunkX, chunkZ)
                                                   : groundChunk(chunkX, chunkZ);
        rng::RandomSource generator = rng::RandomSource::worldgen(rng::RandomSource::legacy(kSeeds[index]));
        const bool placed = treeFeature::place(chunk, generator, chunk.chunkX() * 16 + localX, originY,
                                               chunk.chunkZ() * 16 + localZ, config);
        result.placed += placed ? "1" : "0";
        const Counts counts = fold(chunk);
        result.logs += counts.logs;
        result.leaves += counts.leaves;
        result.decorations += counts.decorations;
        checksum = (checksum ^ static_cast<std::uint64_t>(counts.checksum)) * 1099511628211ULL;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.entities += chunk.blockEntityCount() + chunk.takeGeneratedBlockEntityWrites().size();
        result.after = generator.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("whole trees match the frozen kotlin, configuration by configuration") {
    SUBCASE("oak") {
        const Run result = run(treeConfigs::oak(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 43);
        CHECK(result.leaves == 432);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -1194399152319782907LL);
        CHECK(result.after == -999051014);
    }

    SUBCASE("oakEdge") {
        const Run result = run(treeConfigs::oak(), 71, Ground::Ground, 1, 14);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 43);
        CHECK(result.leaves == 315);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 117);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -5393041882164896897LL);
        CHECK(result.after == -999051014);
    }

    SUBCASE("swampOak") {
        const Run result = run(treeConfigs::swampOak(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 49);
        CHECK(result.leaves == 1072);
        CHECK(result.decorations == 519);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -5351019725141409490LL);
        CHECK(result.after == 148710611);
    }

    SUBCASE("birch") {
        const Run result = run(treeConfigs::birch(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 51);
        CHECK(result.leaves == 432);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 4851093181637399647LL);
        CHECK(result.after == -999051014);
    }

    SUBCASE("fancyOak") {
        const Run result = run(treeConfigs::fancyOak(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 107);
        CHECK(result.leaves == 1568);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 2602438865184189103LL);
        CHECK(result.after == -1227943783);
    }

    SUBCASE("jungleTree") {
        const Run result = run(treeConfigs::jungleTree(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 70);
        CHECK(result.leaves == 432);
        CHECK(result.decorations == 499);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 9147453067671959320LL);
        CHECK(result.after == -714775738);
    }

    SUBCASE("jungleBush") {
        const Run result = run(treeConfigs::jungleBush(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 8);
        CHECK(result.leaves == 229);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 4087777142399298175LL);
        CHECK(result.after == 755100514);
    }

    SUBCASE("megaJungle") {
        const Run result = run(treeConfigs::megaJungle(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 643);
        CHECK(result.leaves == 2136);
        CHECK(result.decorations == 1745);
        CHECK(result.deferred == 65);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -4127809422482040807LL);
        CHECK(result.after == -266944613);
    }

    SUBCASE("spruce") {
        const Run result = run(treeConfigs::spruce(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 58);
        CHECK(result.leaves == 565);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -721531971071746043LL);
        CHECK(result.after == -1320795031);
    }

    SUBCASE("pine") {
        const Run result = run(treeConfigs::pine(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 68);
        CHECK(result.leaves == 288);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 6360835582487715509LL);
        CHECK(result.after == 1125084371);
    }

    SUBCASE("acacia") {
        const Run result = run(treeConfigs::acacia(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 72);
        CHECK(result.leaves == 608);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -476427067433294347LL);
        CHECK(result.after == 383312261);
    }

    SUBCASE("darkOak") {
        const Run result = run(treeConfigs::darkOak(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 354);
        CHECK(result.leaves == 972);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -6988667986974352747LL);
        CHECK(result.after == -1931756187);
    }

    SUBCASE("paleOakCreaking") {
        const Run result = run(treeConfigs::paleOakCreaking(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 354);
        CHECK(result.leaves == 972);
        CHECK(result.decorations == 609);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -1211248588145595467LL);
        CHECK(result.after == 101411853);
    }

    SUBCASE("paleOakCreakingNeighbour") {
        const Run result = run(treeConfigs::paleOakCreaking(), 71, Ground::Creaking, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 649);
        CHECK(result.leaves == 898);
        CHECK(result.decorations == 527);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 2);
        CHECK(result.checksum == 4975010064339921265LL);
        CHECK(result.after == 71405766);
    }

    SUBCASE("megaSpruce") {
        const Run result = run(treeConfigs::megaSpruce(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 596);
        CHECK(result.leaves == 2684);
        CHECK(result.decorations == 931);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -3228967253065688355LL);
        CHECK(result.after == -1931756187);
    }

    SUBCASE("megaPine") {
        const Run result = run(treeConfigs::megaPine(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 596);
        CHECK(result.leaves == 940);
        CHECK(result.decorations == 931);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -2793512005397452735LL);
        CHECK(result.after == -1931756187);
    }

    SUBCASE("cherry") {
        const Run result = run(treeConfigs::cherry(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 108);
        CHECK(result.leaves == 1585);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -2856751508339058074LL);
        CHECK(result.after == 926377866);
    }

    SUBCASE("azalea") {
        const Run result = run(treeConfigs::azalea(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 62);
        CHECK(result.leaves == 748);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -4333738885373896647LL);
        CHECK(result.after == -166352700);
    }

    SUBCASE("oakLeafLitter") {
        const Run result = run(treeConfigs::oakLeafLitter(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 43);
        CHECK(result.leaves == 432);
        CHECK(result.decorations == 248);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 9049155331982306187LL);
        CHECK(result.after == -1471146614);
    }

    SUBCASE("fancyOakBees") {
        const Run result = run(treeConfigs::fancyOakBees(), 71, Ground::Ground, 8, 8);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 107);
        CHECK(result.leaves == 1568);
        CHECK(result.decorations == 8);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 8);
        CHECK(result.checksum == 5461045763849228601LL);
        CHECK(result.after == -1814055817);
    }

    SUBCASE("mangrove") {
        const Run result = run(treeConfigs::mangrove(), 71, Ground::Swamp, 8, 8);
        CHECK(result.placed == "11011111");
        CHECK(result.logs == 51);
        CHECK(result.leaves == 936);
        CHECK(result.decorations == 365);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 56204193917414355LL);
        CHECK(result.after == -695897682);
    }

    SUBCASE("tallMangrove") {
        const Run result = run(treeConfigs::tallMangrove(), 71, Ground::Swamp, 8, 8);
        CHECK(result.placed == "10000001");
        CHECK(result.logs == 36);
        CHECK(result.leaves == 646);
        CHECK(result.decorations == 315);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == 638610873204400791LL);
        CHECK(result.after == 1018495569);
    }

    SUBCASE("blocked") {
        const Run result = run(treeConfigs::oak(), 71, Ground::Ground, 14, 3);
        CHECK(result.placed == "00000000");
        CHECK(result.logs == 0);
        CHECK(result.leaves == 0);
        CHECK(result.decorations == 0);
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.checksum == -2411287437377600371LL);
        CHECK(result.after == 1644477171);
    }
}
