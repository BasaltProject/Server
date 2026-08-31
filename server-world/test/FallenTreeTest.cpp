#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/FallenTreeFeature.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using namespace basalt::server::world::terrain::decoration::tree;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Ground with a step and a pit, so the fall probe and the gap rule both get exercised.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t top = x >= 10 ? 72 : 70;
            for (std::int32_t y = -64; y <= top; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, top, z, stateIdOf("grass_block"));
        }
    }
    for (std::int32_t x = 4; x <= 5; ++x) {
        for (std::int32_t z = 6; z <= 9; ++z) {
            for (std::int32_t y = 66; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
        }
    }
    return chunk;
}

struct Counts {
    std::int32_t logs = 0;
    std::int32_t vines = 0;
    std::int32_t mushrooms = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Counts fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Counts result;
    for (std::int32_t y = 60; y <= 90; ++y) {
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
                if (path == "vine") {
                    ++result.vines;
                } else if (path == "red_mushroom" || path == "brown_mushroom") {
                    ++result.mushrooms;
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
    std::int32_t vines = 0;
    std::int32_t mushrooms = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(const FallenTreeConfiguration& config, std::int32_t localX, std::int32_t localZ,
                      std::int32_t originY) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::size_t index = 0; index < kSeeds.size(); ++index) {
        const std::int32_t step = static_cast<std::int32_t>(index);
        Chunk chunk = groundChunk(step * 5 - 30, step * -3 + 12);
        rng::RandomSource generator = rng::RandomSource::worldgen(rng::RandomSource::legacy(kSeeds[index]));
        const bool placed = fallenTreeFeature::place(chunk, generator, chunk.chunkX() * 16 + localX, originY,
                                                     chunk.chunkZ() * 16 + localZ, config);
        result.placed += placed ? "1" : "0";
        const Counts counts = fold(chunk);
        result.logs += counts.logs;
        result.vines += counts.vines;
        result.mushrooms += counts.mushrooms;
        checksum = (checksum ^ static_cast<std::uint64_t>(counts.checksum)) * 1099511628211ULL;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = generator.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the fallen trees match the frozen kotlin") {
    SUBCASE("oak") {
        const Run result = run(fallenTreeConfigs::oak(), 7, 8, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 25);
        CHECK(result.vines == 21);
        CHECK(result.mushrooms == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3234382624305973141LL);
        CHECK(result.after == 1159570266);
    }

    SUBCASE("oakEdge") {
        const Run result = run(fallenTreeConfigs::oak(), 1, 1, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 16);
        CHECK(result.vines == 21);
        CHECK(result.mushrooms == 1);
        CHECK(result.deferred == 4);
        CHECK(result.checksum == -7482635089775666175LL);
        CHECK(result.after == -1227943783);
    }

    SUBCASE("oakOverPit") {
        const Run result = run(fallenTreeConfigs::oak(), 3, 8, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 28);
        CHECK(result.vines == 21);
        CHECK(result.mushrooms == 1);
        CHECK(result.deferred == 2);
        CHECK(result.checksum == 8326271424227522099LL);
        CHECK(result.after == -1227943783);
    }

    SUBCASE("birch") {
        const Run result = run(fallenTreeConfigs::birch(), 7, 8, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 32);
        CHECK(result.vines == 0);
        CHECK(result.mushrooms == 2);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -197894503810220077LL);
        CHECK(result.after == 346492119);
    }

    SUBCASE("superBirch") {
        const Run result = run(fallenTreeConfigs::superBirch(), 7, 8, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 32);
        CHECK(result.vines == 0);
        CHECK(result.mushrooms == 1);
        CHECK(result.deferred == 5);
        CHECK(result.checksum == -4314168846162319068LL);
        CHECK(result.after == 1809951391);
    }

    SUBCASE("spruce") {
        const Run result = run(fallenTreeConfigs::spruce(), 7, 8, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 37);
        CHECK(result.vines == 0);
        CHECK(result.mushrooms == 2);
        CHECK(result.deferred == 4);
        CHECK(result.checksum == 1451031959472135638LL);
        CHECK(result.after == 1809951391);
    }

    SUBCASE("jungle") {
        const Run result = run(fallenTreeConfigs::jungle(), 7, 8, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 36);
        CHECK(result.vines == 21);
        CHECK(result.mushrooms == 4);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1251417845718184077LL);
        CHECK(result.after == 1159570266);
    }

    SUBCASE("onStep") {
        const Run result = run(fallenTreeConfigs::spruce(), 9, 8, 71);
        CHECK(result.placed == "11111111");
        CHECK(result.logs == 44);
        CHECK(result.vines == 0);
        CHECK(result.mushrooms == 3);
        CHECK(result.deferred == 4);
        CHECK(result.checksum == -6471427007400619165LL);
        CHECK(result.after == 1809951391);
    }
}
