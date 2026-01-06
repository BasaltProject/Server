#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/PlacedFeatures.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// Every placed feature the registry answers to, in the order the frozen Kotlin lists them.
/// The features that need more than a chunk, which answer to `contextual` instead.
constexpr std::array<std::string_view, 6> kContextualIds{"chorus_plant", "end_spike",        "fossil_lower",
                                                         "fossil_upper", "freeze_top_layer", "rooted_sulfur_spring"};

constexpr std::array<std::string_view, 259> kIds{"acacia",
                                                 "acacia_checked",
                                                 "amethyst_geode",
                                                 "bamboo",
                                                 "bamboo_light",
                                                 "bamboo_vegetation",
                                                 "basalt_blobs",
                                                 "basalt_pillar",
                                                 "birch_bees_0002",
                                                 "birch_bees_0002_leaf_litter",
                                                 "birch_bees_002",
                                                 "birch_checked",
                                                 "birch_leaf_litter",
                                                 "birch_tall",
                                                 "blackstone_blobs",
                                                 "blue_ice",
                                                 "brown_mushroom_nether",
                                                 "brown_mushroom_normal",
                                                 "brown_mushroom_old_growth",
                                                 "brown_mushroom_swamp",
                                                 "brown_mushroom_taiga",
                                                 "cave_vines",
                                                 "cherry_bees_005",
                                                 "cherry_checked",
                                                 "classic_vines_cave_feature",
                                                 "crimson_forest_vegetation",
                                                 "crimson_fungi",
                                                 "dark_forest_vegetation",
                                                 "dark_oak_checked",
                                                 "dark_oak_leaf_litter",
                                                 "delta",
                                                 "desert_well",
                                                 "disk_clay",
                                                 "disk_grass",
                                                 "disk_gravel",
                                                 "disk_sand",
                                                 "dripstone_cluster",
                                                 "end_gateway_return",
                                                 "end_island_decorated",
                                                 "end_platform",
                                                 "fallen_birch_tree",
                                                 "fallen_jungle_tree",
                                                 "fallen_oak_tree",
                                                 "fallen_spruce_tree",
                                                 "fallen_super_birch_tree",
                                                 "fancy_oak_bees",
                                                 "fancy_oak_bees_0002_leaf_litter",
                                                 "fancy_oak_bees_002",
                                                 "fancy_oak_checked",
                                                 "fancy_oak_leaf_litter",
                                                 "flower_cherry",
                                                 "flower_default",
                                                 "flower_flower_forest",
                                                 "flower_forest_flowers",
                                                 "flower_meadow",
                                                 "flower_pale_garden",
                                                 "flower_plain",
                                                 "flower_plains",
                                                 "flower_swamp",
                                                 "flower_warm",
                                                 "forest_flowers",
                                                 "forest_rock",
                                                 "glow_lichen",
                                                 "glowstone",
                                                 "glowstone_extra",
                                                 "grass_bonemeal",
                                                 "huge_brown_mushroom",
                                                 "huge_red_mushroom",
                                                 "ice_patch",
                                                 "ice_spike",
                                                 "iceberg_blue",
                                                 "iceberg_packed",
                                                 "jungle_bush",
                                                 "jungle_tree",
                                                 "kelp_cold",
                                                 "kelp_warm",
                                                 "lake_lava_surface",
                                                 "lake_lava_underground",
                                                 "large_basalt_columns",
                                                 "large_dripstone",
                                                 "lush_caves_ceiling_vegetation",
                                                 "lush_caves_clay",
                                                 "lush_caves_vegetation",
                                                 "mangrove_checked",
                                                 "mega_jungle_tree_checked",
                                                 "mega_pine_checked",
                                                 "mega_spruce_checked",
                                                 "monster_room",
                                                 "monster_room_deep",
                                                 "mushroom_island_vegetation",
                                                 "nether_sprouts",
                                                 "oak",
                                                 "oak_bees_0002_leaf_litter",
                                                 "oak_bees_002",
                                                 "oak_checked",
                                                 "oak_leaf_litter",
                                                 "ore_ancient_debris_large",
                                                 "ore_andesite_lower",
                                                 "ore_andesite_upper",
                                                 "ore_blackstone",
                                                 "ore_clay",
                                                 "ore_coal_lower",
                                                 "ore_coal_upper",
                                                 "ore_copper",
                                                 "ore_copper_large",
                                                 "ore_debris_small",
                                                 "ore_diamond",
                                                 "ore_diamond_buried",
                                                 "ore_diamond_large",
                                                 "ore_diamond_medium",
                                                 "ore_diorite_lower",
                                                 "ore_diorite_upper",
                                                 "ore_dirt",
                                                 "ore_emerald",
                                                 "ore_gold",
                                                 "ore_gold_deltas",
                                                 "ore_gold_extra",
                                                 "ore_gold_lower",
                                                 "ore_gold_nether",
                                                 "ore_granite_lower",
                                                 "ore_granite_upper",
                                                 "ore_gravel",
                                                 "ore_gravel_nether",
                                                 "ore_infested",
                                                 "ore_iron_middle",
                                                 "ore_iron_small",
                                                 "ore_iron_upper",
                                                 "ore_lapis",
                                                 "ore_lapis_buried",
                                                 "ore_magma",
                                                 "ore_quartz_deltas",
                                                 "ore_quartz_nether",
                                                 "ore_redstone",
                                                 "ore_redstone_lower",
                                                 "ore_soul_sand",
                                                 "ore_tuff",
                                                 "pale_garden_flowers",
                                                 "pale_garden_vegetation",
                                                 "pale_moss_patch",
                                                 "pale_oak_checked",
                                                 "pale_oak_creaking_checked",
                                                 "patch_berry_bush",
                                                 "patch_berry_common",
                                                 "patch_berry_rare",
                                                 "patch_bush",
                                                 "patch_cactus",
                                                 "patch_cactus_decorated",
                                                 "patch_cactus_desert",
                                                 "patch_crimson_roots",
                                                 "patch_dead_bush",
                                                 "patch_dead_bush_2",
                                                 "patch_dead_bush_badlands",
                                                 "patch_dry_grass_badlands",
                                                 "patch_dry_grass_desert",
                                                 "patch_fire",
                                                 "patch_firefly_bush_near_water",
                                                 "patch_firefly_bush_near_water_swamp",
                                                 "patch_firefly_bush_swamp",
                                                 "patch_grass_badlands",
                                                 "patch_grass_forest",
                                                 "patch_grass_jungle",
                                                 "patch_grass_meadow",
                                                 "patch_grass_normal",
                                                 "patch_grass_plain",
                                                 "patch_grass_savanna",
                                                 "patch_grass_taiga",
                                                 "patch_grass_taiga_2",
                                                 "patch_large_fern",
                                                 "patch_leaf_litter",
                                                 "patch_melon",
                                                 "patch_melon_sparse",
                                                 "patch_pumpkin",
                                                 "patch_soul_fire",
                                                 "patch_sugar_cane",
                                                 "patch_sugar_cane_badlands",
                                                 "patch_sugar_cane_desert",
                                                 "patch_sugar_cane_swamp",
                                                 "patch_sunflower",
                                                 "patch_taiga_grass",
                                                 "patch_tall_grass",
                                                 "patch_tall_grass_2",
                                                 "patch_waterlily",
                                                 "pile_hay",
                                                 "pile_ice",
                                                 "pile_melon",
                                                 "pile_pumpkin",
                                                 "pile_snow",
                                                 "pine",
                                                 "pine_checked",
                                                 "pine_on_snow",
                                                 "pointed_dripstone",
                                                 "red_mushroom_nether",
                                                 "red_mushroom_normal",
                                                 "red_mushroom_old_growth",
                                                 "red_mushroom_swamp",
                                                 "red_mushroom_taiga",
                                                 "rooted_azalea_tree",
                                                 "sculk_patch_ancient_city",
                                                 "sculk_patch_deep_dark",
                                                 "sculk_vein",
                                                 "sea_pickle",
                                                 "seagrass_cold",
                                                 "seagrass_deep",
                                                 "seagrass_deep_cold",
                                                 "seagrass_deep_warm",
                                                 "seagrass_normal",
                                                 "seagrass_river",
                                                 "seagrass_swamp",
                                                 "seagrass_warm",
                                                 "small_basalt_columns",
                                                 "spore_blossom",
                                                 "spring_closed",
                                                 "spring_closed_double",
                                                 "spring_delta",
                                                 "spring_lava",
                                                 "spring_lava_frozen",
                                                 "spring_lava_nether",
                                                 "spring_open",
                                                 "spring_water",
                                                 "spruce",
                                                 "spruce_checked",
                                                 "spruce_on_snow",
                                                 "sulfur_pool",
                                                 "sulfur_spike",
                                                 "sulfur_spike_cluster",
                                                 "super_birch_bees",
                                                 "super_birch_bees_0002",
                                                 "tall_mangrove_checked",
                                                 "trees_badlands",
                                                 "trees_birch",
                                                 "trees_birch_and_oak_leaf_litter",
                                                 "trees_cherry",
                                                 "trees_flower_forest",
                                                 "trees_grove",
                                                 "trees_jungle",
                                                 "trees_mangrove",
                                                 "trees_meadow",
                                                 "trees_old_growth_pine_taiga",
                                                 "trees_old_growth_spruce_taiga",
                                                 "trees_plains",
                                                 "trees_savanna",
                                                 "trees_snowy",
                                                 "trees_sparse_jungle",
                                                 "trees_swamp",
                                                 "trees_taiga",
                                                 "trees_water",
                                                 "trees_windswept_forest",
                                                 "trees_windswept_hills",
                                                 "trees_windswept_savanna",
                                                 "twisting_vines",
                                                 "underwater_magma",
                                                 "vines",
                                                 "void_start_platform",
                                                 "warm_ocean_vegetation",
                                                 "warped_forest_vegetation",
                                                 "warped_fungi",
                                                 "weeping_vines",
                                                 "wildflowers_birch_forest",
                                                 "wildflowers_meadow"};

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// Enough of a world for most features: rock, soil, a lake, a cavern and open sky.
[[nodiscard]] Chunk worldChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            const bool lake = floorMod(worldX * 3 + worldZ * 5, 11) < 3;
            const std::int32_t surface = lake ? 60 : 70;
            for (std::int32_t y = 0; y < surface; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (std::int32_t y = surface - 4; y < surface; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("dirt"));
            }
            chunk.setBlockStateId(x, surface, z,
                                  lake ? stateIdOf("sand") : registry::blocks::named("grass_block")->stateId);
            if (lake) {
                for (std::int32_t y = surface + 1; y <= 63; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
                }
            }
            for (std::int32_t y = 20; y <= 27; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
            for (std::int32_t y = 10; y <= 17; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("netherrack"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::int64_t fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t y = 0; y <= 110; ++y) {
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

} // namespace

TEST_CASE("the placed feature registry answers to the frozen kotlin's ids") {
    SUBCASE("runnable from a chunk alone") {
        std::vector<std::string> found;
        for (const basalt::wire::common::Identifier& id : placedFeatures::ids()) {
            if (placedFeatures::find(id) != nullptr) {
                found.push_back(id.path());
            }
        }
        std::vector<std::string> expected{kIds.begin(), kIds.end()};
        CHECK(found.size() == expected.size());
        CHECK(found == expected);
    }

    SUBCASE("needing the rest of the world") {
        std::vector<std::string> found;
        for (const basalt::wire::common::Identifier& id : placedFeatures::contextualIds()) {
            found.push_back(id.path());
        }
        std::vector<std::string> expected{kContextualIds.begin(), kContextualIds.end()};
        CHECK(found.size() == expected.size());
        CHECK(found == expected);
        // The two halves are disjoint and together are everything the registry answers to.
        CHECK(placedFeatures::ids().size() == kIds.size() + kContextualIds.size());
    }
}

TEST_CASE("every placed feature places what the frozen kotlin places") {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::size_t deferred = 0;
    std::int32_t after = 0;
    for (std::size_t index = 0; index < kIds.size(); ++index) {
        const placedFeatures::Placement* placement =
            placedFeatures::find(basalt::wire::common::Identifier::vanilla(std::string{kIds[index]}));
        REQUIRE(placement != nullptr);
        const auto step = static_cast<std::int32_t>(index);
        Chunk chunk = worldChunk(floorMod(step, 3) - 1, floorMod(step, 5) + 2);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(3491LL + step * 761LL));
        // Two heights per feature: the open surface and the cavern floor underground.
        for (const std::int32_t y : {71, 21}) {
            (*placement)(chunk, random, chunk.chunkX() * 16 + 8, y, chunk.chunkZ() * 16 + 8);
        }
        checksum = (checksum ^ static_cast<std::uint64_t>(fold(chunk))) * 1099511628211ULL;
        deferred += chunk.takeGeneratedBlockWrites().size();
        after = random.nextInt();
    }
    CHECK(deferred == 71);
    CHECK(static_cast<std::int64_t>(checksum) == 4539318681170738607LL);
    CHECK(after == -1699011601);
}
