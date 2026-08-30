package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment

/** Runs the 106 Overworld VEGETAL_DECORATION nodes in FeatureSorter index order. */
public object OverworldVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
    ) {
        for (feature in FEATURES) feature.run(chunk, seed, environment)
    }

    internal fun featureOrder(): List<Pair<Int, String>> = FEATURES.map { it.index to it.name }

    private data class Feature(
        val index: Int,
        val name: String,
        val run: (Chunk, Long, SurfaceRuleEnvironment) -> Unit,
    )

    private fun feature(
        index: Int,
        name: String,
        run: (Chunk, Long, SurfaceRuleEnvironment) -> Unit,
    ): Feature = Feature(index, name, run)

    private fun tree(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            TreePatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun flower(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            FlowerPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun lush(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            LushCaveVegetationDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun doublePlant(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            DoublePlantPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun groundPlant(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            GroundPlantPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun mushroom(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            MushroomPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun sugarCane(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            SugarCanePatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun cactus(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            CactusPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun firefly(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            FireflyBushPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun aquatic(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            AquaticPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun melon(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            MelonPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private fun berry(
        index: Int,
        name: String,
    ): Feature =
        feature(index, name) { chunk, seed, environment ->
            BerryPatchDecoration.decorateFeature(chunk, seed, environment, name)
        }

    private val FEATURES =
        listOf(
            feature(0, "glow_lichen", GlowLichenDecoration::decorate),
            tree(1, "trees_windswept_savanna"),
            feature(2, "bamboo") { chunk, seed, environment ->
                BambooDecoration.decorateFeature(2, chunk, seed, environment)
            },
            tree(3, "bamboo_vegetation"),
            tree(4, "trees_sparse_jungle"),
            tree(5, "trees_badlands"),
            feature(6, "bamboo_light") { chunk, seed, environment ->
                BambooDecoration.decorateFeature(6, chunk, seed, environment)
            },
            tree(7, "trees_jungle"),
            feature(8, "patch_tall_grass") { chunk, seed, environment ->
                TallGrassDecoration.decorateFeature(8, chunk, seed, environment)
            },
            tree(9, "trees_savanna"),
            flower(10, "flower_warm"),
            feature(11, "patch_grass_jungle") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(11, chunk, seed, environment)
            },
            feature(12, "patch_grass_savanna") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(12, chunk, seed, environment)
            },
            tree(13, "pale_garden_vegetation"),
            feature(14, "pale_moss_patch") { chunk, seed, environment ->
                PaleMossDecoration.decorateFeature(chunk, seed, environment, null)
            },
            flower(15, "pale_garden_flowers"),
            flower(16, "flower_pale_garden"),
            tree(17, "dark_forest_vegetation"),
            flower(18, "flower_forest_flowers"),
            tree(19, "trees_flower_forest"),
            flower(20, "flower_flower_forest"),
            flower(21, "forest_flowers"),
            flower(22, "wildflowers_birch_forest"),
            tree(23, "birch_tall"),
            tree(24, "trees_birch"),
            tree(25, "trees_birch_and_oak_leaf_litter"),
            feature(26, "patch_tall_grass_2") { chunk, seed, environment ->
                TallGrassDecoration.decorateFeature(26, chunk, seed, environment)
            },
            lush(27, "lush_caves_ceiling_vegetation"),
            lush(28, "cave_vines"),
            lush(29, "lush_caves_clay"),
            lush(30, "lush_caves_vegetation"),
            lush(31, "rooted_azalea_tree"),
            lush(32, "spore_blossom"),
            lush(33, "classic_vines_cave_feature"),
            doublePlant(34, "patch_sunflower"),
            feature(35, "patch_grass_meadow") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(35, chunk, seed, environment)
            },
            flower(36, "flower_meadow"),
            tree(37, "trees_meadow"),
            flower(38, "wildflowers_meadow"),
            tree(39, "trees_windswept_forest"),
            doublePlant(40, "patch_large_fern"),
            tree(41, "trees_old_growth_pine_taiga"),
            tree(42, "trees_old_growth_spruce_taiga"),
            tree(43, "trees_taiga"),
            tree(44, "trees_grove"),
            tree(45, "trees_windswept_hills"),
            tree(46, "trees_snowy"),
            tree(47, "trees_mangrove"),
            tree(48, "trees_swamp"),
            flower(49, "flower_swamp"),
            tree(50, "trees_water"),
            feature(51, "patch_bush", BushPatchDecoration::decorate),
            tree(52, "trees_plains"),
            flower(53, "flower_plains"),
            feature(54, "patch_grass_plain") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(54, chunk, seed, environment)
            },
            flower(55, "flower_cherry"),
            tree(56, "trees_cherry"),
            flower(57, "flower_default"),
            feature(58, "patch_grass_taiga") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(58, chunk, seed, environment)
            },
            feature(59, "patch_grass_forest") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(59, chunk, seed, environment)
            },
            feature(60, "patch_grass_taiga_2") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(60, chunk, seed, environment)
            },
            feature(61, "patch_grass_normal") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(61, chunk, seed, environment)
            },
            groundPlant(62, "patch_dead_bush"),
            mushroom(63, "brown_mushroom_old_growth"),
            mushroom(64, "red_mushroom_old_growth"),
            feature(65, "patch_waterlily", WaterlilyPatchDecoration::decorate),
            mushroom(66, "brown_mushroom_swamp"),
            mushroom(67, "red_mushroom_swamp"),
            feature(68, "patch_grass_badlands") { chunk, seed, environment ->
                GrassPatchDecoration.decorateFeature(68, chunk, seed, environment)
            },
            groundPlant(69, "patch_dry_grass_desert"),
            groundPlant(70, "patch_dead_bush_2"),
            groundPlant(71, "patch_dry_grass_badlands"),
            groundPlant(72, "patch_dead_bush_badlands"),
            mushroom(73, "brown_mushroom_normal"),
            mushroom(74, "red_mushroom_normal"),
            sugarCane(75, "patch_sugar_cane_desert"),
            sugarCane(76, "patch_sugar_cane_badlands"),
            feature(77, "patch_leaf_litter", LeafLitterPatchDecoration::decorate),
            sugarCane(78, "patch_sugar_cane_swamp"),
            feature(79, "mushroom_island_vegetation") { chunk, seed, environment ->
                MushroomFieldsDecoration.decorateFeature(chunk, seed, environment, null)
            },
            mushroom(80, "brown_mushroom_taiga"),
            mushroom(81, "red_mushroom_taiga"),
            feature(82, "patch_pumpkin", PumpkinPatchDecoration::decorate),
            cactus(83, "patch_cactus_desert"),
            cactus(84, "patch_cactus_decorated"),
            firefly(85, "patch_firefly_bush_swamp"),
            firefly(86, "patch_firefly_bush_near_water_swamp"),
            aquatic(87, "seagrass_swamp"),
            sugarCane(88, "patch_sugar_cane"),
            firefly(89, "patch_firefly_bush_near_water"),
            aquatic(90, "seagrass_river"),
            feature(91, "vines", VinePatchDecoration::decorate),
            melon(92, "patch_melon_sparse"),
            melon(93, "patch_melon"),
            berry(94, "patch_berry_common"),
            berry(95, "patch_berry_rare"),
            feature(96, "warm_ocean_vegetation") { chunk, seed, environment ->
                CoralDecoration.decorateFeature(chunk, seed, environment, null)
            },
            aquatic(97, "seagrass_warm"),
            aquatic(98, "sea_pickle"),
            aquatic(99, "seagrass_deep_warm"),
            aquatic(100, "kelp_warm"),
            aquatic(101, "seagrass_normal"),
            aquatic(102, "seagrass_deep"),
            aquatic(103, "seagrass_cold"),
            aquatic(104, "seagrass_deep_cold"),
            aquatic(105, "kelp_cold"),
        ).also { features ->
            check(features.map(Feature::index) == (0..105).toList())
        }
}
