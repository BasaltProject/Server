#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/LushCaveVegetationDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::FixedBiomeEnvironment;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

constexpr std::array<std::int32_t, 3> kCavernBottoms{-40, -12, 16};
constexpr std::int32_t kCavernHeight = 10;

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// Stone caverns with a water pool and a dirt band, which is what the clay and moss want.
[[nodiscard]] Chunk caveChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            for (std::int32_t y = -64; y < 40; ++y) {
                const bool dirt = floorMod(worldX * 3 + y * 5 + worldZ, 17) == 0;
                chunk.setBlockStateId(x, y, z, stateIdOf(dirt ? "dirt" : "stone"));
            }
            for (const std::int32_t bottom : kCavernBottoms) {
                for (std::int32_t y = bottom; y < bottom + kCavernHeight; ++y) {
                    // A shallow pool on one cavern floor: the clay patch only forms under water.
                    const bool flooded = bottom == -12 && y < bottom + 2;
                    chunk.setBlockStateId(
                        x, y, z, flooded ? registry::blocks::water().stateId : registry::blocks::air().stateId);
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ignored = {"stone", "dirt", "air", "water"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = -64; y < 40; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && !ignored.contains(block->path())) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

struct Run {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::string_view biome, std::string_view feature, std::int32_t count) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    for (std::int32_t chunkIndex = 0; chunkIndex < count; ++chunkIndex) {
        Chunk chunk = caveChunk(chunkIndex * 4 - 5, chunkIndex * -2 + 3);
        if (feature.empty()) {
            lushCaveVegetationDecoration::decorate(chunk, 2131LL + chunkIndex, environment);
        } else {
            lushCaveVegetationDecoration::decorateFeature(chunk, 2131LL + chunkIndex, environment, feature);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the lush cave step places what the frozen kotlin places") {
    SUBCASE("wholeStep") {
        const Run result = run("minecraft:lush_caves", "", 3);
        CHECK(result.placed == 8832);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 2206301739624513804LL);
    }

    SUBCASE("plainsStep") {
        const Run result = run("minecraft:plains", "", 3);
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 195630881141514078LL);
    }

    SUBCASE("lush_caves_ceiling_vegetation") {
        const Run result = run("minecraft:lush_caves", "lush_caves_ceiling_vegetation", 3);
        CHECK(result.placed == 3151);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -9094470158408238628LL);
    }

    SUBCASE("cave_vines") {
        const Run result = run("minecraft:lush_caves", "cave_vines", 3);
        CHECK(result.placed == 177);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7777776524992525772LL);
    }

    SUBCASE("lush_caves_clay") {
        const Run result = run("minecraft:lush_caves", "lush_caves_clay", 3);
        CHECK(result.placed == 4434);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5297415231206651918LL);
    }

    SUBCASE("lush_caves_vegetation") {
        const Run result = run("minecraft:lush_caves", "lush_caves_vegetation", 3);
        CHECK(result.placed == 2225);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1191217117782258872LL);
    }

    SUBCASE("rooted_azalea_tree") {
        const Run result = run("minecraft:lush_caves", "rooted_azalea_tree", 48);
        CHECK(result.placed == 1833);
        CHECK(result.deferred == 117);
        CHECK(result.checksum == 8955489135532821344LL);
    }

    SUBCASE("spore_blossom") {
        const Run result = run("minecraft:lush_caves", "spore_blossom", 3);
        CHECK(result.placed == 5);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 9180669688352651443LL);
    }

    SUBCASE("classic_vines_cave_feature") {
        const Run result = run("minecraft:lush_caves", "classic_vines_cave_feature", 3);
        CHECK(result.placed == 10);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 9118025491837563198LL);
    }
}
