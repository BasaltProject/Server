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
             // parity: a growth reaches the face of a block in the next chunk, so it needs the
             // same neighbour view every other step gets. Without it every border face reads as
             // nothing and the lichen goes somewhere else.
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
        {5, "trees_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_badlands");
         }},
        {6, "bamboo_light",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { bambooDecoration::decorateFeature(6, chunk, seed, environment); }},
        {7, "trees_jungle",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_jungle");
         }},
        {8, "patch_tall_grass",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { tallGrassDecoration::decorateFeature(8, chunk, seed, environment); }},
        {9, "trees_savanna",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_savanna");
         }},
        {10, "flower_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_warm");
         }},
        {11, "patch_grass_jungle",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(11, chunk, seed, environment); }},
        {12, "patch_grass_savanna",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(12, chunk, seed, environment); }},
        {13, "pale_garden_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "pale_garden_vegetation");
         }},
        {14, "pale_moss_patch",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { paleMossDecoration::decorate(chunk, seed, environment); }},
        {15, "pale_garden_flowers",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "pale_garden_flowers");
         }},
        {16, "flower_pale_garden",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_pale_garden");
         }},
        {17, "dark_forest_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "dark_forest_vegetation");
         }},
        {18, "flower_forest_flowers",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_forest_flowers");
         }},
        {19, "trees_flower_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_flower_forest");
         }},
        {20, "flower_flower_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_flower_forest");
         }},
        {21, "forest_flowers",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "forest_flowers");
         }},
        {22, "wildflowers_birch_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "wildflowers_birch_forest");
         }},
        {23, "birch_tall",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "birch_tall");
         }},
        {24, "trees_birch",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_birch");
         }},
        {25, "trees_birch_and_oak_leaf_litter",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_birch_and_oak_leaf_litter");
         }},
        {26, "patch_tall_grass_2",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { tallGrassDecoration::decorateFeature(26, chunk, seed, environment); }},
        {27, "lush_caves_ceiling_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "lush_caves_ceiling_vegetation");
         }},
        {28, "cave_vines",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "cave_vines");
         }},
        {29, "lush_caves_clay",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "lush_caves_clay");
         }},
        {30, "lush_caves_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "lush_caves_vegetation");
         }},
        {31, "rooted_azalea_tree",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "rooted_azalea_tree");
         }},
        {32, "spore_blossom",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "spore_blossom");
         }},
        {33, "classic_vines_cave_feature",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             lushCaveVegetationDecoration::decorateFeature(chunk, seed, environment, "classic_vines_cave_feature");
         }},
        {34, "patch_sunflower",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             doublePlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_sunflower");
         }},
        {35, "patch_grass_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(35, chunk, seed, environment); }},
        {36, "flower_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_meadow");
         }},
        {37, "trees_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_meadow");
         }},
        {38, "wildflowers_meadow",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "wildflowers_meadow");
         }},
        {39, "trees_windswept_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_windswept_forest");
         }},
        {40, "patch_large_fern",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             doublePlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_large_fern");
         }},
        {41, "trees_old_growth_pine_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_old_growth_pine_taiga");
         }},
        {42, "trees_old_growth_spruce_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_old_growth_spruce_taiga");
         }},
        {43, "trees_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_taiga");
         }},
        {44, "trees_grove",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_grove");
         }},
        {45, "trees_windswept_hills",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_windswept_hills");
         }},
        {46, "trees_snowy",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_snowy");
         }},
        {47, "trees_mangrove",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_mangrove");
         }},
        {48, "trees_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_swamp");
         }},
        {49, "flower_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_swamp");
         }},
        {50, "trees_water",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_water");
         }},
        {51, "patch_bush",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { bushPatchDecoration::decorate(chunk, seed, environment); }},
        {52, "trees_plains",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_plains");
         }},
        {53, "flower_plains",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_plains");
         }},
        {54, "patch_grass_plain",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(54, chunk, seed, environment); }},
        {55, "flower_cherry",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_cherry");
         }},
        {56, "trees_cherry",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             treePatchDecoration::decorateFeature(chunk, seed, environment, "trees_cherry");
         }},
        {57, "flower_default",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             flowerPatchDecoration::decorateFeature(chunk, seed, environment, "flower_default");
         }},
        {58, "patch_grass_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(58, chunk, seed, environment); }},
        {59, "patch_grass_forest",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(59, chunk, seed, environment); }},
        {60, "patch_grass_taiga_2",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(60, chunk, seed, environment); }},
        {61, "patch_grass_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(61, chunk, seed, environment); }},
        {62, "patch_dead_bush",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dead_bush");
         }},
        {63, "brown_mushroom_old_growth",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_old_growth");
         }},
        {64, "red_mushroom_old_growth",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_old_growth");
         }},
        {65, "patch_waterlily",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { waterlilyPatchDecoration::decorate(chunk, seed, environment); }},
        {66, "brown_mushroom_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_swamp");
         }},
        {67, "red_mushroom_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_swamp");
         }},
        {68, "patch_grass_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { grassPatchDecoration::decorateFeature(68, chunk, seed, environment); }},
        {69, "patch_dry_grass_desert",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dry_grass_desert");
         }},
        {70, "patch_dead_bush_2",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dead_bush_2");
         }},
        {71, "patch_dry_grass_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dry_grass_badlands");
         }},
        {72, "patch_dead_bush_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             groundPlantPatchDecoration::decorateFeature(chunk, seed, environment, "patch_dead_bush_badlands");
         }},
        {73, "brown_mushroom_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_normal");
         }},
        {74, "red_mushroom_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_normal");
         }},
        {75, "patch_sugar_cane_desert",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane_desert");
         }},
        {76, "patch_sugar_cane_badlands",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane_badlands");
         }},
        {77, "patch_leaf_litter",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { leafLitterPatchDecoration::decorate(chunk, seed, environment); }},
        {78, "patch_sugar_cane_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane_swamp");
         }},
        {79, "mushroom_island_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { mushroomFieldsDecoration::decorate(chunk, seed, environment); }},
        {80, "brown_mushroom_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "brown_mushroom_taiga");
         }},
        {81, "red_mushroom_taiga",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             mushroomPatchDecoration::decorateFeature(chunk, seed, environment, "red_mushroom_taiga");
         }},
        {82, "patch_pumpkin",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { pumpkinPatchDecoration::decorate(chunk, seed, environment); }},
        {83, "patch_cactus_desert",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             cactusPatchDecoration::decorateFeature(chunk, seed, environment, "patch_cactus_desert");
         }},
        {84, "patch_cactus_decorated",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             cactusPatchDecoration::decorateFeature(chunk, seed, environment, "patch_cactus_decorated");
         }},
        {85, "patch_firefly_bush_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             fireflyBushPatchDecoration::decorateFeature(chunk, seed, environment, "patch_firefly_bush_swamp");
         }},
        {86, "patch_firefly_bush_near_water_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             fireflyBushPatchDecoration::decorateFeature(chunk, seed, environment,
                                                         "patch_firefly_bush_near_water_swamp");
         }},
        {87, "seagrass_swamp",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_swamp");
         }},
        {88, "patch_sugar_cane",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             sugarCanePatchDecoration::decorateFeature(chunk, seed, environment, "patch_sugar_cane");
         }},
        {89, "patch_firefly_bush_near_water",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             fireflyBushPatchDecoration::decorateFeature(chunk, seed, environment, "patch_firefly_bush_near_water");
         }},
        {90, "seagrass_river",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_river");
         }},
        {91, "vines",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { vinePatchDecoration::decorate(chunk, seed, environment); }},
        {92, "patch_melon_sparse",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             melonPatchDecoration::decorateFeature(chunk, seed, environment, "patch_melon_sparse");
         }},
        {93, "patch_melon",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             melonPatchDecoration::decorateFeature(chunk, seed, environment, "patch_melon");
         }},
        {94, "patch_berry_common",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             berryPatchDecoration::decorateFeature(chunk, seed, environment, "patch_berry_common");
         }},
        {95, "patch_berry_rare",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             berryPatchDecoration::decorateFeature(chunk, seed, environment, "patch_berry_rare");
         }},
        {96, "warm_ocean_vegetation",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) { coralDecoration::decorate(chunk, seed, environment); }},
        {97, "seagrass_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_warm");
         }},
        {98, "sea_pickle",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "sea_pickle");
         }},
        {99, "seagrass_deep_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_deep_warm");
         }},
        {100, "kelp_warm",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "kelp_warm");
         }},
        {101, "seagrass_normal",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_normal");
         }},
        {102, "seagrass_deep",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_deep");
         }},
        {103, "seagrass_cold",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_cold");
         }},
        {104, "seagrass_deep_cold",
         [](Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
            const ExternalStateReader&) {
             aquaticPatchDecoration::decorateFeature(chunk, seed, environment, "seagrass_deep_cold");
         }},
        {105, "kelp_cold",
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
