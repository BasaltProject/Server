package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.world.terrain.decoration.tree.FallenTreeConfigs

/** One biome's real `vegetal_decoration` (and friends) generation step. // parity: the `Decoration.VEGETAL_DECORATION`-and-later portion of a biome's own `BiomeGenerationSettings`. */
public fun interface BiomeVegetalDecoration {
    public fun decorate(
        chunk: Chunk,
        random: WorldgenRandom,
    )
}

/** Runs each of [steps] in order against the same [chunk]/[random]: matches how a real biome's own `BiomeGenerationSettings` runs its `VEGETAL_DECORATION` features one after another over a single shared random stream. */
public fun combine(vararg steps: BiomeVegetalDecoration): BiomeVegetalDecoration =
    BiomeVegetalDecoration { chunk, random -> steps.forEach { it.decorate(chunk, random) } }

/**
 * Dispatches a biome identifier to its real decoration, matching real vanilla's own
 * `ChunkGenerator.applyBiomeDecoration` (which looks up the actual biome at each decorated
 * position, not a single fixed one). [org.basaltmc.world.terrain.RealTerrainGenerator] resolves
 * only the chunk's own center column and decorates the whole chunk with that one result: real
 * vanilla's decoration is chunk-scoped in practice too (everything placed by one chunk's
 * `applyBiomeDecoration` call shares that chunk's carved region), and this project's own decoration
 * pipeline is already chunk-scoped ([PlacementContext][org.basaltmc.world.terrain.decoration.PlacementContext]
 * has no cross-chunk state), so per-column dispatch is not meaningfully different except at biome
 * borders, an accepted, documented approximation.
 *
 * All 55 current Overworld biome identifiers are listed. Unknown biome identifiers resolve to
 * `null` rather than borrowing another biome's feature list.
 */
public object BiomeDecoration {
    private val noVegetalDecoration = BiomeVegetalDecoration { _, _ -> }
    private val oakTree = TreeDecoration.tree(TreeConfigs.OAK)
    private val birchTree = TreeDecoration.tree(TreeConfigs.BIRCH)
    private val superBirchTree = TreeDecoration.tree(TreeConfigs.SUPER_BIRCH)
    private val spruceTree = TreeDecoration.tree(TreeConfigs.SPRUCE)
    private val pineTree = TreeDecoration.tree(TreeConfigs.PINE)
    private val fancyOakTree = TreeDecoration.tree(TreeConfigs.FANCY_OAK)
    private val fallenOakTree = TreeDecoration.fallen(FallenTreeConfigs.OAK)
    private val fallenBirchTree = TreeDecoration.fallen(FallenTreeConfigs.BIRCH)
    private val fallenSuperBirchTree = TreeDecoration.fallen(FallenTreeConfigs.SUPER_BIRCH)
    private val fallenSpruceTree = TreeDecoration.fallen(FallenTreeConfigs.SPRUCE)
    private val fallenJungleTree = TreeDecoration.fallen(FallenTreeConfigs.JUNGLE)

    private val byBiome: Map<Identifier, BiomeVegetalDecoration> =
        mapOf(
            // parity: TREES_PLAINS, fancy oak 33.3%, fallen oak 1.25%, oak default.
            Identifier.vanilla("plains") to
                TreeDecoration.candidateSequenceCountExtra(listOf(0.33333334f to fancyOakTree, 0.0125f to fallenOakTree), oakTree, 0, 0.05f, 1),
            Identifier.vanilla("sunflower_plains") to
                TreeDecoration.candidateSequenceCountExtra(listOf(0.33333334f to fancyOakTree, 0.0125f to fallenOakTree), oakTree, 0, 0.05f, 1),
            // parity: TREES_BIRCH_AND_OAK_LEAF_LITTER; leaf-litter and beehive decorators remain shared gaps.
            Identifier.vanilla("forest") to
                TreeDecoration.candidateSequenceCountExtra(
                    listOf(0.0025f to fallenBirchTree, 0.2f to birchTree, 0.1f to fancyOakTree, 0.0125f to fallenOakTree),
                    oakTree,
                    10,
                    0.1f,
                    1,
                ),
            // parity: OverworldBiomes.forest(flower=true): trees_flower_forest, flower_flower_forest,
            // and default grass. Fallen birch and tree decorators remain shared gaps.
            Identifier.vanilla("flower_forest") to FlowerForestDecoration,
            // parity: OverworldBiomes.forest(birch=true, tall=false)/addBirchTrees: TREES_BIRCH, birch default.
            Identifier.vanilla("birch_forest") to
                TreeDecoration.candidateSequenceCountExtra(listOf(0.0125f to fallenBirchTree), birchTree, 10, 0.1f, 1),
            // parity: OverworldBiomes.forest(birch=true, tall=true)/addTallBirchTrees: BIRCH_TALL, super_birch default.
            Identifier.vanilla("old_growth_birch_forest") to
                TreeDecoration.candidateSequenceCountExtra(
                    listOf(0.00625f to fallenSuperBirchTree, 0.5f to superBirchTree, 0.0125f to fallenBirchTree),
                    birchTree,
                    10,
                    0.1f,
                    1,
                ),
            // parity: OverworldBiomes.taiga/addTaigaTrees+addDefaultFlowers: TREES_TAIGA (33.3% pine else spruce,
            // fallen_spruce 1.25%) plus the real default flower patch. Shared ground cover and the
            // taiga mushroom tail are appended by forBiome.
            Identifier.vanilla("taiga") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(0.33333334f to pineTree, 0.0125f to fallenSpruceTree),
                        spruceTree,
                        10,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.FLOWERS,
                ),
            // parity: OverworldBiomes.taiga(snowy=true)/addSnowyTrees+addDefaultFlowers: TREES_SNOWY, spruce default.
            Identifier.vanilla("snowy_taiga") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(listOf(0.0125f to fallenSpruceTree), spruceTree, 0, 0.1f, 1),
                    DefaultVegetationDecoration.FLOWERS,
                ),
            // parity: OverworldBiomes.oldGrowthTaiga(spruce=true)/TREES_OLD_GROWTH_SPRUCE_TAIGA: 33.3% mega_spruce,
            // 33.3% pine, fallen_spruce 1.25%, spruce default. countExtra(10, 0.1, 1).
            Identifier.vanilla("old_growth_spruce_taiga") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(
                            0.33333334f to TreeDecoration.tree(TreeConfigs.MEGA_SPRUCE),
                            0.33333334f to pineTree,
                            0.0125f to fallenSpruceTree,
                        ),
                        spruceTree,
                        10,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.FLOWERS,
                ),
            // parity: OverworldBiomes.oldGrowthTaiga(spruce=false)/TREES_OLD_GROWTH_PINE_TAIGA: 2.56% mega_spruce,
            // 30.8% mega_pine, 33.3% pine, fallen_spruce 1.25%, spruce default.
            Identifier.vanilla("old_growth_pine_taiga") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(
                            0.025641026f to TreeDecoration.tree(TreeConfigs.MEGA_SPRUCE),
                            0.30769232f to TreeDecoration.tree(TreeConfigs.MEGA_PINE),
                            0.33333334f to pineTree,
                            0.0125f to fallenSpruceTree,
                        ),
                        spruceTree,
                        10,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.FLOWERS,
                ),
            // parity: OverworldBiomes.plains(snowy=true, spikes=false/true)/addSnowyTrees: same TREES_SNOWY feature.
            Identifier.vanilla("snowy_plains") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(listOf(0.0125f to fallenSpruceTree), spruceTree, 0, 0.1f, 1),
                    DefaultVegetationDecoration,
                ),
            Identifier.vanilla("ice_spikes") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(listOf(0.0125f to fallenSpruceTree), spruceTree, 0, 0.1f, 1),
                    DefaultVegetationDecoration,
                ),
            // parity: OverworldBiomes.savanna/addSavannaTrees+addWarmFlowers: TREES_SAVANNA (80% acacia else oak,
            // fallen_oak 1.25%) plus flower_warm (real flowerDefault, rarity 1/16).
            Identifier.vanilla("savanna") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(0.8f to TreeDecoration.tree(TreeConfigs.ACACIA), 0.0125f to fallenOakTree),
                        oakTree,
                        1,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.WARM_FLOWERS,
                ),
            // parity: OverworldBiomes.savanna(plateau=true)/addSavannaTrees+addWarmFlowers: same as plains savanna.
            Identifier.vanilla("savanna_plateau") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(0.8f to TreeDecoration.tree(TreeConfigs.ACACIA), 0.0125f to fallenOakTree),
                        oakTree,
                        1,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.WARM_FLOWERS,
                ),
            // parity: OverworldBiomes.savanna(shattered=true)/addShatteredSavannaTrees+addDefaultFlowers (not
            // addWarmFlowers: the shattered variant uses the real default rarity, not the warm one).
            Identifier.vanilla("windswept_savanna") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(0.8f to TreeDecoration.tree(TreeConfigs.ACACIA), 0.0125f to fallenOakTree),
                        oakTree,
                        2,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.FLOWERS,
                ),
            // parity: TREES_JUNGLE; all selector shapes are present, decorators remain shared gaps.
            Identifier.vanilla("jungle") to
                combine(
                    BambooDecoration.LIGHT,
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(
                            0.1f to fancyOakTree,
                            0.5f to TreeDecoration.tree(TreeConfigs.JUNGLE_BUSH),
                            0.33333334f to TreeDecoration.tree(TreeConfigs.MEGA_JUNGLE),
                            0.0125f to fallenJungleTree,
                        ),
                        TreeDecoration.tree(TreeConfigs.JUNGLE_TREE),
                        50,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.WARM_FLOWERS,
                    JungleGrassDecoration,
                ),
            // parity: TREES_SPARSE_JUNGLE; all selector shapes are present, decorators remain shared gaps.
            Identifier.vanilla("sparse_jungle") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(0.1f to fancyOakTree, 0.5f to TreeDecoration.tree(TreeConfigs.JUNGLE_BUSH), 0.0125f to fallenJungleTree),
                        TreeDecoration.tree(TreeConfigs.JUNGLE_TREE),
                        2,
                        0.1f,
                        1,
                    ),
                    DefaultVegetationDecoration.WARM_FLOWERS,
                    JungleGrassDecoration,
                ),
            // parity: OverworldBiomes.windsweptHills(moreTrees=false)/addMountainTrees+addDefaultFlowers+addDefaultGrass:
            // TREES_WINDSWEPT_HILLS: fallen spruce 0.8325%, spruce 66.6%, fancy oak 10%, fallen oak 1.25%, oak default.
            Identifier.vanilla("windswept_hills") to
                TreeDecoration.candidateSequenceCountExtra(
                    listOf(0.008325f to fallenSpruceTree, 0.666f to spruceTree, 0.1f to fancyOakTree, 0.0125f to fallenOakTree),
                    oakTree,
                    0,
                    0.1f,
                    1,
                ),
            Identifier.vanilla("windswept_gravelly_hills") to
                TreeDecoration.candidateSequenceCountExtra(
                    listOf(0.008325f to fallenSpruceTree, 0.666f to spruceTree, 0.1f to fancyOakTree, 0.0125f to fallenOakTree),
                    oakTree,
                    0,
                    0.1f,
                    1,
                ),
            // parity: OverworldBiomes.windsweptHills(moreTrees=true)/addMountainForestTrees: TREES_WINDSWEPT_FOREST, same species mix.
            Identifier.vanilla("windswept_forest") to
                TreeDecoration.candidateSequenceCountExtra(
                    listOf(0.008325f to fallenSpruceTree, 0.666f to spruceTree, 0.1f to fancyOakTree, 0.0125f to fallenOakTree),
                    oakTree,
                    3,
                    0.1f,
                    1,
                ),
            // parity: OverworldBiomes.grove/addGroveTrees: TREES_GROVE, 33.3% pine else spruce (real "on snow" placement filter dropped).
            Identifier.vanilla("grove") to TreeDecoration.choice(TreeConfigs.PINE, 0.33333334f, TreeConfigs.SPRUCE, 10, 0.1f, 1),
            // parity: peaks have no vegetal features; snowy slopes adds only patch_pumpkin later.
            // Explicit empty bases keep their earlier shared disk stage reachable through forBiome.
            Identifier.vanilla("snowy_slopes") to noVegetalDecoration,
            Identifier.vanilla("frozen_peaks") to noVegetalDecoration,
            Identifier.vanilla("jagged_peaks") to noVegetalDecoration,
            Identifier.vanilla("stony_peaks") to noVegetalDecoration,
            // parity: OverworldBiomes.badlands(wooded=true)/addBadlandsTrees+addBadlandGrass: TREES_BADLANDS (oak
            // default, oak_leaf_litter/fallen_oak simplified to plain oak) plus patch_grass_badlands (the same
            // real feature DefaultVegetationDecoration.GRASS already builds) and patch_dead_bush_badlands.
            Identifier.vanilla("wooded_badlands") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(listOf(0.0125f to fallenOakTree), oakTree, 5, 0.1f, 1),
                    DefaultVegetationDecoration.GRASS,
                    BadlandsDecoration,
                ),
            // parity: OverworldBiomes.badlands(wooded=false)/addBadlandGrass; specialized ground cover is appended by forBiome.
            Identifier.vanilla("badlands") to combine(DefaultVegetationDecoration.GRASS, BadlandsDecoration),
            Identifier.vanilla("eroded_badlands") to combine(DefaultVegetationDecoration.GRASS, BadlandsDecoration),
            // parity: OverworldBiomes.swamp/addSwampVegetation: TREES_SWAMP (swamp_oak, a single feature, not a
            // random selector) plus flower_swamp/patch_grass_normal/patch_dead_bush/patch_waterlily.
            Identifier.vanilla("swamp") to combine(TreeDecoration.single(TreeConfigs.SWAMP_OAK, 2, 0.1f, 1), SwampDecoration),
            // parity: OverworldBiomes.desert/addDesertVegetation+addDesertExtraVegetation: patch_cactus_desert, patch_dead_bush_2.
            Identifier.vanilla("desert") to DesertDecoration,
            // parity: OverworldBiomes.darkForest(isPaleGarden=false)/DARK_FOREST_VEGETATION: real RANDOM_SELECTOR,
            // huge brown 2.5%, huge red 5%, dark_oak 66.7%, fallen_birch 0.25%, birch 20%, fallen_oak
            // 1.25%, fancy_oak 10%, oak default. CountPlacement(16), not countExtra.
            Identifier.vanilla("dark_forest") to
                TreeDecoration.candidateSequence(
                    listOf(
                        0.025f to TreeDecoration.hugeMushroom(HugeMushroomFeature.Kind.BROWN),
                        0.05f to TreeDecoration.hugeMushroom(HugeMushroomFeature.Kind.RED),
                        0.6666667f to TreeDecoration.tree(TreeConfigs.DARK_OAK),
                        0.0025f to fallenBirchTree,
                        0.2f to birchTree,
                        0.0125f to fallenOakTree,
                        0.1f to fancyOakTree,
                    ),
                    oakTree,
                    16,
                ),
            // parity: OverworldBiomes.darkForest(isPaleGarden=true)/PALE_GARDEN_VEGETATION: 10% pale_oak_creaking
            // (same tree shape as the default, only the CreakingHeartDecorator differs, not ported), else pale_oak.
            // CountPlacement(16). Pale moss and pale garden flowers are appended by forBiome.
            Identifier.vanilla("pale_garden") to TreeDecoration.fixed(TreeConfigs.PALE_OAK, 16),
            // parity: meadow tall grass, grass, DualNoiseProvider flowers, rare trees, and wildflowers.
            Identifier.vanilla("meadow") to MeadowDecoration,
            // parity: OverworldBiomes.meadowOrCherryGrove(cherryGrove=true)/addCherryGroveVegetation:
            // patch_grass_plain + flower_cherry (pink petals) + trees_cherry (countExtra(10, 0.1, 1),
            // cherry_bees_005 minus the unported BeehiveDecorator). See CherryGroveDecoration's own KDoc.
            Identifier.vanilla("cherry_grove") to CherryGroveDecoration,
            // parity: river vegetation is composed in forBiome so water trees precede bushes and defaults.
            Identifier.vanilla("river") to combine(DefaultVegetationDecoration, AquaticDecoration.RIVER),
            Identifier.vanilla("frozen_river") to DefaultVegetationDecoration,
            // parity: OverworldBiomes.beach(snowy=false, stony=false)/addDefaultFlowers+addDefaultGrass.
            Identifier.vanilla("beach") to DefaultVegetationDecoration,
            Identifier.vanilla("snowy_beach") to DefaultVegetationDecoration,
            Identifier.vanilla("stony_shore") to DefaultVegetationDecoration,
            // parity: OverworldBiomes.bambooJungle/baseJungle: addWarmFlowers+addJungleGrass (shared with
            // jungle/sparse_jungle), after dense bamboo and the bamboo_vegetation selector.
            Identifier.vanilla("bamboo_jungle") to
                combine(BambooDecoration.DENSE, BambooVegetationDecoration, DefaultVegetationDecoration.WARM_FLOWERS, JungleGrassDecoration),
            // parity: OverworldBiomes.dripstoneCaves/deepDark: both call addPlainGrass+addPlainVegetation, the
            // exact same real features Plains itself uses (PlainsDecoration already builds them). Real vanilla
            // also runs addDripstone/addSculk here (cave-specific structures, not ported); this project has no
            // cave generation yet either, so these two biomes are effectively unreachable at the surface right
            // now, but the real ground cover they'd get once caves exist is already correct.
            Identifier.vanilla("dripstone_caves") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(0.33333334f to fancyOakTree, 0.0125f to fallenOakTree),
                        oakTree,
                        0,
                        0.05f,
                        1,
                    ),
                    PlainsDecoration,
                ),
            Identifier.vanilla("deep_dark") to
                combine(
                    TreeDecoration.candidateSequenceCountExtra(
                        listOf(0.33333334f to fancyOakTree, 0.0125f to fallenOakTree),
                        oakTree,
                        0,
                        0.05f,
                        1,
                    ),
                    PlainsDecoration,
                ),
            // parity: OverworldBiomes.lushCaves/sulfurCaves: both call only addPlainGrass (real patch_tall_grass_2),
            // not the rest of Plains' own decoration. addLushCavesVegetationFeatures/addSulfurCavesFeatures (cave
            // ceiling vegetation, glow berries, azalea, sulfur spikes) are real cave-specific systems, not ported;
            // like dripstone_caves/deep_dark, neither biome is reachable without cave generation yet either.
            Identifier.vanilla("lush_caves") to PlainsDecoration.TALL_GRASS,
            Identifier.vanilla("sulfur_caves") to PlainsDecoration.TALL_GRASS,
            // parity: OverworldBiomes.mangroveSwamp/addMangroveSwampVegetation: TREES_MANGROVE (85% tall_mangrove
            // else mangrove, CountPlacement(25) fixed) plus patch_grass_normal/patch_dead_bush/patch_waterlily
            // (the same real features SwampDecoration.GROUND_COVER already builds, no flower_swamp here).
            // Real MangroveRootPlacer ported: trunks start elevated on simulated roots.
            Identifier.vanilla("mangrove_swamp") to
                combine(TreeDecoration.sequence(listOf(0.85f to TreeConfigs.TALL_MANGROVE), TreeConfigs.MANGROVE, 25), SwampDecoration.GROUND_COVER),
            // parity: OverworldBiomes.mushroomFields/addMushroomFieldVegetation: mushroom_island_vegetation
            // (huge red/brown 50/50, one attempt per chunk) + brown_mushroom_taiga (rarity 4) +
            // red_mushroom_taiga (rarity 256). Shared near-water vegetation is appended by forBiome.
            Identifier.vanilla("mushroom_fields") to MushroomFieldsDecoration,
            // parity: OverworldBiomes.ocean/coldOcean/lukeWarmOcean. Each variant selects the
            // matching real seagrass count/tall probability, then its cold or warm kelp density.
            Identifier.vanilla("ocean") to AquaticDecoration.OCEAN,
            Identifier.vanilla("deep_ocean") to AquaticDecoration.DEEP_OCEAN,
            Identifier.vanilla("cold_ocean") to AquaticDecoration.COLD_OCEAN,
            Identifier.vanilla("deep_cold_ocean") to AquaticDecoration.DEEP_COLD_OCEAN,
            Identifier.vanilla("lukewarm_ocean") to AquaticDecoration.LUKEWARM_OCEAN,
            Identifier.vanilla("deep_lukewarm_ocean") to AquaticDecoration.DEEP_LUKEWARM_OCEAN,
            // parity: warm ocean adds coral first, then seagrass_warm and sea_pickle.
            Identifier.vanilla("warm_ocean") to AquaticDecoration.WARM_OCEAN,
            // parity: frozenOcean adds packed/blue icebergs in LOCAL_MODIFICATIONS, blue ice in
            // SURFACE_STRUCTURES, then water trees and the same default vegetation as beaches.
            Identifier.vanilla("frozen_ocean") to DefaultVegetationDecoration,
            Identifier.vanilla("deep_frozen_ocean") to DefaultVegetationDecoration,
        )

    public fun forBiome(id: Identifier): BiomeVegetalDecoration? {
        val path = id.path
        val body = when (path) {
            "plains" ->
                combine(
                    PlainsDecoration.TALL_GRASS,
                    ForestGroundDecoration.BUSH,
                    requireBase(id),
                    PlainsDecoration.FLOWERS,
                    PlainsDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "sunflower_plains" ->
                combine(
                    PlainsDecoration.TALL_GRASS,
                    PlainsDecoration.SUNFLOWER,
                    requireBase(id),
                    PlainsDecoration.FLOWERS,
                    PlainsDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "ocean" -> oceanWithExtra(AquaticDecoration.OCEAN)
            "deep_ocean" -> oceanWithExtra(AquaticDecoration.DEEP_OCEAN)
            "cold_ocean" -> oceanWithExtra(AquaticDecoration.COLD_OCEAN)
            "deep_cold_ocean" -> oceanWithExtra(AquaticDecoration.DEEP_COLD_OCEAN)
            "lukewarm_ocean" -> oceanWithExtra(AquaticDecoration.LUKEWARM_OCEAN)
            "deep_lukewarm_ocean" -> oceanWithExtra(AquaticDecoration.DEEP_LUKEWARM_OCEAN)
            "warm_ocean" -> oceanWithExtra(AquaticDecoration.WARM_OCEAN)
            "frozen_ocean", "deep_frozen_ocean" ->
                combine(
                    TreeDecoration.WATER,
                    requireBase(id),
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "river" ->
                combine(
                    TreeDecoration.WATER,
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                    AquaticDecoration.RIVER,
                )
            "frozen_river" ->
                combine(
                    TreeDecoration.WATER,
                    ForestGroundDecoration.BUSH,
                    requireBase(id),
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "swamp" ->
                combine(
                    requireBase(id),
                    MushroomDecoration.SWAMP,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.SWAMP,
                    AquaticDecoration.SWAMP,
                )
            "mangrove_swamp" ->
                combine(requireBase(id), AquaticDecoration.SWAMP, ExtraVegetationDecoration.MANGROVE)
            "desert" -> combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.DESERT)
            "badlands", "eroded_badlands", "wooded_badlands" ->
                combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.BADLANDS)
            "mushroom_fields" -> combine(requireBase(id), ExtraVegetationDecoration.NEAR_WATER)
            "jungle", "bamboo_jungle" ->
                combine(
                    requireBase(id),
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                    JungleVineDecoration,
                    JungleMelonDecoration.NORMAL,
                )
            "sparse_jungle" ->
                combine(
                    requireBase(id),
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                    JungleVineDecoration,
                    JungleMelonDecoration.SPARSE,
                )
            "savanna", "savanna_plateau" ->
                combine(
                    SavannaGroundDecoration.TALL_GRASS,
                    requireBase(id),
                    SavannaGroundDecoration.SAVANNA,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "windswept_savanna" ->
                combine(
                    requireBase(id),
                    SavannaGroundDecoration.WINDSWEPT,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "forest" ->
                combine(
                    ForestFlowerDecoration.NORMAL,
                    requireBase(id),
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration.FLOWERS,
                    ForestGroundDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "birch_forest", "old_growth_birch_forest" ->
                combine(
                    ForestFlowerDecoration.NORMAL,
                    ForestFloorDecoration.BIRCH_WILDFLOWERS,
                    requireBase(id),
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration.FLOWERS,
                    ForestGroundDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "dark_forest" ->
                combine(
                    requireBase(id),
                    ForestFlowerDecoration.NORMAL,
                    DefaultVegetationDecoration.FLOWERS,
                    ForestGroundDecoration.GRASS,
                    MushroomDecoration.NORMAL,
                    ForestFloorDecoration.LEAF_LITTER,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "pale_garden" ->
                combine(
                    requireBase(id),
                    PaleMossDecoration,
                    PaleGardenDecoration.FLOWER_PATCH,
                    PaleGardenDecoration.RARE_FLOWER,
                    ForestGroundDecoration.GRASS,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "flower_forest" ->
                combine(
                    ForestFlowerDecoration.FLOWER_FOREST,
                    requireBase(id),
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            "windswept_hills", "windswept_gravelly_hills", "windswept_forest" ->
                combine(
                    requireBase(id),
                    ForestGroundDecoration.BUSH,
                    DefaultVegetationDecoration,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                )
            in OLD_GROWTH_TAIGA_PATHS ->
                combine(
                    TaigaGroundDecoration.LARGE_FERN,
                    requireBase(id),
                    TaigaGroundDecoration.OLD_GROWTH,
                    MushroomDecoration.OLD_GROWTH,
                    MushroomDecoration.NORMAL,
                    ExtraVegetationDecoration.DEFAULT,
                    TaigaGroundDecoration.COMMON_BERRY,
                )
            "taiga" ->
                combine(
                    TaigaGroundDecoration.LARGE_FERN,
                    requireBase(id),
                    TaigaGroundDecoration.NORMAL,
                    MushroomDecoration.TAIGA,
                    ExtraVegetationDecoration.DEFAULT,
                    TaigaGroundDecoration.COMMON_BERRY,
                )
            "snowy_taiga" ->
                combine(
                    TaigaGroundDecoration.LARGE_FERN,
                    requireBase(id),
                    TaigaGroundDecoration.NORMAL,
                    MushroomDecoration.TAIGA,
                    ExtraVegetationDecoration.DEFAULT,
                    TaigaGroundDecoration.RARE_BERRY,
                )
            in DEFAULT_EXTRA_PATHS ->
                if (path == "pale_garden") {
                    combine(requireBase(id), ExtraVegetationDecoration.DEFAULT)
                } else {
                    combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.DEFAULT)
                }
            in PUMPKIN_ONLY_PATHS ->
                if (path == "dripstone_caves" || path == "deep_dark") {
                    combine(requireBase(id), MushroomDecoration.NORMAL, ExtraVegetationDecoration.PUMPKIN_ONLY)
                } else {
                    combine(requireBase(id), ExtraVegetationDecoration.PUMPKIN_ONLY)
                }
            else -> byBiome[id]
        }
        body ?: return null
        return when (path) {
            "ice_spikes" -> combine(IceSpikesDecoration, SoftDiskDecoration.DEFAULT, GlowLichenDecoration, body)
            "frozen_ocean", "deep_frozen_ocean" -> combine(FrozenOceanDecoration, SoftDiskDecoration.DEFAULT, GlowLichenDecoration, body)
            "swamp" -> combine(SoftDiskDecoration.SWAMP, GlowLichenDecoration, body)
            "mangrove_swamp" -> combine(SoftDiskDecoration.MANGROVE, GlowLichenDecoration, body)
            else -> combine(SoftDiskDecoration.DEFAULT, GlowLichenDecoration, body)
        }
    }

    private fun oceanWithExtra(aquatic: BiomeVegetalDecoration): BiomeVegetalDecoration =
        combine(TreeDecoration.WATER, DefaultVegetationDecoration, MushroomDecoration.NORMAL, ExtraVegetationDecoration.DEFAULT, aquatic)

    private fun requireBase(id: Identifier): BiomeVegetalDecoration =
        byBiome[id] ?: error("missing base decoration for $id")

    private val DEFAULT_EXTRA_PATHS: Set<String> =
        setOf(
            "snowy_plains",
            "ice_spikes",
            "beach",
            "snowy_beach",
            "stony_shore",
            "frozen_ocean",
            "deep_frozen_ocean",
        )

    private val PUMPKIN_ONLY_PATHS: Set<String> =
        setOf("grove", "snowy_slopes", "dripstone_caves", "deep_dark")

    private val OLD_GROWTH_TAIGA_PATHS: Set<String> = setOf("old_growth_pine_taiga", "old_growth_spruce_taiga")
}
