#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldVegetalDecoration.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::FixedBiomeEnvironment;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// A land surface with a lake and a cavern, so land, water and cave features all have a home.
[[nodiscard]] Chunk worldChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            const bool lake = floorMod(worldX * 3 + worldZ * 5, 11) < 3;
            const std::int32_t surface = lake ? 60 : 70;
            for (std::int32_t y = 20; y <= surface - 1; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, surface, z,
                                  lake ? stateIdOf("sand") : registry::blocks::named("grass_block")->stateId);
            if (lake) {
                for (std::int32_t y = surface + 1; y <= 63; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
                }
            }
            // A cavern for the lush cave and glow lichen entries.
            for (std::int32_t y = 30; y <= 37; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
            if (floorMod(worldX + worldZ, 5) == 0) {
                chunk.setBlockStateId(x, 30, z, stateIdOf("dirt"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ignored = {"stone", "grass_block", "sand", "dirt", "air", "water"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 20; y <= 110; ++y) {
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

[[nodiscard]] Run run(std::string_view biome) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
        Chunk chunk = worldChunk(chunkIndex * 2 - 3, chunkIndex + 4);
        overworldVegetalDecoration::decorate(chunk, 4157LL + chunkIndex, environment);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the overworld vegetal step runs its nodes in the jar's feature order") {
    // parity: `FeatureSorter` over the 26.3 biomes, step 9. The three dappled forest features have
    // their slots but no node yet.
    static constexpr std::string_view kOrder =
        "glow_lichen,trees_windswept_savanna,bamboo,bamboo_vegetation,trees_sparse_jungle,trees_dappled_forest,br"
        "own_mushroom_dappled_forest,patch_red_shrub,trees_badlands,bamboo_light,trees_jungle,patch_tall_grass,tr"
        "ees_savanna,flower_warm,patch_grass_jungle,patch_grass_savanna,pale_garden_vegetation,pale_moss_patch,pa"
        "le_garden_flowers,flower_pale_garden,dark_forest_vegetation,flower_forest_flowers,trees_flower_forest,fl"
        "ower_flower_forest,forest_flowers,wildflowers_birch_forest,birch_tall,trees_birch,trees_birch_and_oak_le"
        "af_litter,patch_tall_grass_2,lush_caves_ceiling_vegetation,cave_vines,lush_caves_clay,lush_caves_vegetat"
        "ion,rooted_azalea_tree,spore_blossom,classic_vines_cave_feature,patch_sunflower,patch_grass_meadow,flowe"
        "r_meadow,trees_meadow,wildflowers_meadow,trees_windswept_forest,patch_large_fern,trees_old_growth_pine_t"
        "aiga,trees_old_growth_spruce_taiga,trees_taiga,trees_grove,trees_windswept_hills,trees_snowy,trees_mangr"
        "ove,trees_swamp,flower_swamp,trees_water,patch_bush,trees_plains,flower_plains,patch_grass_plain,flower_"
        "cherry,trees_cherry,flower_default,patch_grass_taiga,patch_grass_forest,patch_grass_taiga_2,patch_grass_"
        "normal,patch_dead_bush,brown_mushroom_old_growth,red_mushroom_old_growth,patch_waterlily,brown_mushroom_"
        "swamp,red_mushroom_swamp,patch_grass_badlands,patch_dry_grass_desert,patch_dead_bush_2,patch_dry_grass_b"
        "adlands,patch_dead_bush_badlands,brown_mushroom_normal,red_mushroom_normal,patch_sugar_cane_desert,patch"
        "_sugar_cane_badlands,patch_leaf_litter,patch_sugar_cane_swamp,mushroom_island_vegetation,brown_mushroom_"
        "taiga,red_mushroom_taiga,patch_pumpkin,patch_cactus_desert,patch_cactus_decorated,patch_firefly_bush_swa"
        "mp,patch_firefly_bush_near_water_swamp,seagrass_swamp,patch_sugar_cane,patch_firefly_bush_near_water,sea"
        "grass_river,vines,patch_melon_sparse,patch_melon,patch_berry_common,patch_berry_rare,warm_ocean_vegetati"
        "on,seagrass_warm,sea_pickle,seagrass_deep_warm,kelp_warm,seagrass_normal,seagrass_deep,seagrass_cold,sea"
        "grass_deep_cold,kelp_cold";
    std::vector<std::string_view> expected;
    for (std::size_t start = 0; start <= kOrder.size();) {
        const std::size_t comma = std::min(kOrder.find(',', start), kOrder.size());
        expected.push_back(kOrder.substr(start, comma - start));
        start = comma + 1;
    }
    REQUIRE(expected.size() == 109);
    const std::set<std::string_view> missing{"trees_dappled_forest", "brown_mushroom_dappled_forest",
                                             "patch_red_shrub"};
    std::size_t next = 0;
    for (const auto& [index, name] : overworldVegetalDecoration::featureOrder()) {
        while (next < expected.size() && missing.contains(expected[next])) {
            ++next;
        }
        REQUIRE(next < expected.size());
        CHECK(name == expected[next]);
        CHECK(index == static_cast<std::int32_t>(next));
        ++next;
    }
    CHECK(overworldVegetalDecoration::featureOrder().size() == expected.size() - missing.size());
}

TEST_CASE("the overworld vegetal step places what the jar places") {
    SUBCASE("plains") {
        const Run result = run("minecraft:plains");
        CHECK(result.placed == 68);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2061496627550257704LL);
    }

    SUBCASE("jungle") {
        const Run result = run("minecraft:jungle");
        CHECK(result.placed == 2971);
        CHECK(result.deferred == 883);
        CHECK(result.checksum == -8973945534636578123LL);
    }

    SUBCASE("lush_caves") {
        const Run result = run("minecraft:lush_caves");
        CHECK(result.placed == 4011);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2518120076210245304LL);
    }

    SUBCASE("swamp") {
        const Run result = run("minecraft:swamp");
        CHECK(result.placed == 756);
        CHECK(result.deferred == 167);
        CHECK(result.checksum == 8064041867149315264LL);
    }

    SUBCASE("warm_ocean") {
        const Run result = run("minecraft:warm_ocean");
        CHECK(result.placed == 56);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 5129051271633250079LL);
    }
}
