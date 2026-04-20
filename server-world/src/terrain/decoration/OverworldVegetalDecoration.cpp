#include <basalt/server/world/terrain/decoration/AquaticPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/BambooDecoration.hpp>
#include <basalt/server/world/terrain/decoration/BushPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/CactusFeature.hpp>
#include <basalt/server/world/terrain/decoration/CoralDecoration.hpp>
#include <basalt/server/world/terrain/decoration/DoublePlantPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/FireflyBushPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/FlowerPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/GlowLichenDecoration.hpp>
#include <basalt/server/world/terrain/decoration/GrassPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/GrassScatterPatches.hpp>
#include <basalt/server/world/terrain/decoration/GroundPlantPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/LushCaveVegetationDecoration.hpp>
#include <basalt/server/world/terrain/decoration/MelonPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/MushroomFieldsDecoration.hpp>
#include <basalt/server/world/terrain/decoration/MushroomPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OverworldVegetalDecoration.hpp>
#include <basalt/server/world/terrain/decoration/PaleMossDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SugarCanePatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SurfacePatches.hpp>
#include <basalt/server/world/terrain/decoration/TallGrassDecoration.hpp>
#include <basalt/server/world/terrain/decoration/TreePatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/VinePatchDecoration.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::decoration {
namespace {

struct Feature {
    std::int32_t index;
    std::string_view name;
    std::function<void(Chunk&, std::int64_t, const surface::SurfaceRuleEnvironment&, const ExternalStateReader&)> run;
};

const std::vector<Feature>& features() {
    static const std::vector<Feature> built = {
        {0, "glow_lichen",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader& externalBlockState) {
             // parity: a growth reaches a block face in the next chunk, so it needs the neighbour view.
             glowLichenDecoration::decorate(chunk, seed, environment, externalBlockState);
         }},
        {1, "trees_windswept_savanna",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_windswept_savanna");
         }},
        {2, "bamboo",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { bambooDecoration::decorateFeature(2, chunk, seed, environment); }},
        {3, "bamboo_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "bamboo_vegetation");
         }},
        {4, "trees_sparse_jungle",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_sparse_jungle");
         }},
        {5, "trees_dappled_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_dappled_forest");
         }},
        {6, "brown_mushroom_dappled_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_dappled_forest");
         }},
        {7, "patch_red_shrub",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_red_shrub");
         }},
        {8, "trees_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_badlands");
         }},
        {9, "bamboo_light",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { bambooDecoration::decorateFeature(9, chunk, seed, environment); }},
        {10, "trees_jungle",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_jungle");
         }},
        {11, "patch_tall_grass",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { tallGrassDecoration::decorateFeature(11, chunk, seed, environment); }},
        {12, "trees_savanna",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_savanna");
         }},
        {13, "flower_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_warm");
         }},
        {14, "patch_grass_jungle",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(14, chunk, seed, environment); }},
        {15, "patch_grass_savanna",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(15, chunk, seed, environment); }},
        {16, "pale_garden_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "pale_garden_vegetation");
         }},
        {17, "pale_moss_patch",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { paleMossDecoration::decorate(chunk, seed, environment); }},
        {18, "pale_garden_flowers",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "pale_garden_flowers");
         }},
        {19, "flower_pale_garden",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_pale_garden");
         }},
        {20, "dark_forest_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "dark_forest_vegetation");
         }},
        {21, "flower_forest_flowers",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_forest_flowers");
         }},
        {22, "trees_flower_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_flower_forest");
         }},
        {23, "flower_flower_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_flower_forest");
         }},
        {24, "forest_flowers",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "forest_flowers");
         }},
        {25, "wildflowers_birch_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "wildflowers_birch_forest");
         }},
        {26, "birch_tall",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "birch_tall");
         }},
        {27, "trees_birch",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_birch");
         }},
        {28, "trees_birch_and_oak_leaf_litter",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_birch_and_oak_leaf_litter");
         }},
        {29, "patch_tall_grass_2",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { tallGrassDecoration::decorateFeature(29, chunk, seed, environment); }},
        {30, "lush_caves_ceiling_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "lush_caves_ceiling_vegetation");
         }},
        {31, "cave_vines",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "cave_vines");
         }},
        {32, "lush_caves_clay",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "lush_caves_clay");
         }},
        {33, "lush_caves_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "lush_caves_vegetation");
         }},
        {34, "rooted_azalea_tree",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "rooted_azalea_tree");
         }},
        {35, "spore_blossom",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "spore_blossom");
         }},
        {36, "classic_vines_cave_feature",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "classic_vines_cave_feature");
         }},
        {37, "patch_sunflower",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             doublePlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_sunflower");
         }},
        {38, "patch_grass_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(38, chunk, seed, environment); }},
        {39, "flower_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_meadow");
         }},
        {40, "trees_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_meadow");
         }},
        {41, "wildflowers_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "wildflowers_meadow");
         }},
        {42, "trees_windswept_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_windswept_forest");
         }},
        {43, "patch_large_fern",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             doublePlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_large_fern");
         }},
        {44, "trees_old_growth_pine_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_old_growth_pine_taiga");
         }},
        {45, "trees_old_growth_spruce_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_old_growth_spruce_taiga");
         }},
        {46, "trees_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_taiga");
         }},
        {47, "trees_grove",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_grove");
         }},
        {48, "trees_windswept_hills",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_windswept_hills");
         }},
        {49, "trees_snowy",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_snowy");
         }},
        {50, "trees_mangrove",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_mangrove");
         }},
        {51, "trees_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_swamp");
         }},
        {52, "flower_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_swamp");
         }},
        {53, "trees_water",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_water");
         }},
        {54, "patch_bush",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { bushPatchDecoration::decorate(chunk, seed, environment); }},
        {55, "trees_plains",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_plains");
         }},
        {56, "flower_plains",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_plains");
         }},
        {57, "patch_grass_plain",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(57, chunk, seed, environment); }},
        {58, "flower_cherry",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_cherry");
         }},
        {59, "trees_cherry",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_cherry");
         }},
        {60, "flower_default",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_default");
         }},
        {61, "patch_grass_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(61, chunk, seed, environment); }},
        {62, "patch_grass_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(62, chunk, seed, environment); }},
        {63, "patch_grass_taiga_2",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(63, chunk, seed, environment); }},
        {64, "patch_grass_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(64, chunk, seed, environment); }},
        {65, "patch_dead_bush",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dead_bush");
         }},
        {66, "brown_mushroom_old_growth",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_old_growth");
         }},
        {67, "red_mushroom_old_growth",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_old_growth");
         }},
        {68, "patch_waterlily",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { waterlilyPatchDecoration::decorate(chunk, seed, environment); }},
        {69, "brown_mushroom_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_swamp");
         }},
        {70, "red_mushroom_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_swamp");
         }},
        {71, "patch_grass_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(71, chunk, seed, environment); }},
        {72, "patch_dry_grass_desert",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dry_grass_desert");
         }},
        {73, "patch_dead_bush_2",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dead_bush_2");
         }},
        {74, "patch_dry_grass_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dry_grass_badlands");
         }},
        {75, "patch_dead_bush_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dead_bush_badlands");
         }},
        {76, "brown_mushroom_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_normal");
         }},
        {77, "red_mushroom_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_normal");
         }},
        {78, "patch_sugar_cane_desert",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane_desert");
         }},
        {79, "patch_sugar_cane_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane_badlands");
         }},
        {80, "patch_leaf_litter",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { leafLitterPatchDecoration::decorate(chunk, seed, environment); }},
        {81, "patch_sugar_cane_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane_swamp");
         }},
        {82, "mushroom_island_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { mushroomFieldsDecoration::decorate(chunk, seed, environment); }},
        {83, "brown_mushroom_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_taiga");
         }},
        {84, "red_mushroom_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_taiga");
         }},
        {85, "patch_pumpkin",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { pumpkinPatchDecoration::decorate(chunk, seed, environment); }},
        {86, "patch_cactus_desert",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             cactusPatchDecoration::decorateFeature(chunk, seed, environment, "patch_cactus_desert");
         }},
        {87, "patch_cactus_decorated",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             cactusPatchDecoration::decorateFeature(chunk, seed, environment, "patch_cactus_decorated");
         }},
        {88, "patch_firefly_bush_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             fireflyBushPatchDecoration::decorateFeature(chunk, seed, environment, "patch_firefly_bush_swamp");
         }},
        {89, "patch_firefly_bush_near_water_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             fireflyBushPatchDecoration::decorateFeature(chunk, seed, environment,
                                                         "patch_firefly_bush_near_water_swamp");
         }},
        {90, "seagrass_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_swamp");
         }},
        {91, "patch_sugar_cane",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane");
         }},
        {92, "patch_firefly_bush_near_water",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             fireflyBushPatchDecoration::decorateFeature(chunk, seed, environment, "patch_firefly_bush_near_water");
         }},
        {93, "seagrass_river",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_river");
         }},
        {94, "vines",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { vinePatchDecoration::decorate(chunk, seed, environment); }},
        {95, "patch_melon_sparse",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             melonPatchDecoration::decorateFeature(chunk, seed, environment, "patch_melon_sparse");
         }},
        {96, "patch_melon",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             melonPatchDecoration::decorateFeature(chunk, seed, environment, "patch_melon");
         }},
        {97, "patch_berry_common",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             berryPatchDecoration::decorateFeature(chunk, seed, environment, "patch_berry_common");
         }},
        {98, "patch_berry_rare",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             berryPatchDecoration::decorateFeature(chunk, seed, environment, "patch_berry_rare");
         }},
        {99, "warm_ocean_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { coralDecoration::decorate(chunk, seed, environment); }},
        {100, "seagrass_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_warm");
         }},
        {101, "sea_pickle",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "sea_pickle");
         }},
        {102, "seagrass_deep_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_deep_warm");
         }},
        {103, "kelp_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "kelp_warm");
         }},
        {104, "seagrass_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_normal");
         }},
        {105, "seagrass_deep",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_deep");
         }},
        {106, "seagrass_cold",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_cold");
         }},
        {107, "seagrass_deep_cold",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_deep_cold");
         }},
        {108, "kelp_cold",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "kelp_cold");
         }},
    };
    return built;
}

} // namespace

const std::vector<std::pair<std::int32_t, std::string_view>>& overworldVegetalDecoration::featureOrder() {
    static const std::vector<std::pair<std::int32_t, std::string_view>> order = [] {
        std::vector<std::pair<std::int32_t, std::string_view>> built;
        built.reserve(features().size());
        for (const Feature& feature : features()) {
            built.emplace_back(feature.index, feature.name);
        }
        return built;
    }();
    return order;
}

void overworldVegetalDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                          const surface::SurfaceRuleEnvironment& environment,
                                          ExternalStateReader externalBlockState) {
    for (const Feature& feature : features()) {
        feature.run(chunk, seed, environment, externalBlockState);
    }
}

} // namespace basalt::server::world::terrain::decoration
