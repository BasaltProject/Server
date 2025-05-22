package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.decoration.tree.TreeConfigs

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
 * Only biomes with real, jar-verified decoration ported so far are listed: every other biome
 * resolves to `null` (no vegetal decoration at all), strictly more correct than running a
 * different biome's own feature list over it.
 */
public object BiomeDecoration {
    private val byBiome: Map<Identifier, BiomeVegetalDecoration> =
        mapOf(
            // parity: OverworldBiomes.plains(sunflower=false)/BiomeDefaultFeatures.addPlainVegetation.
            // TREES_PLAINS: countExtra(0, 0.05, 1), oak default (fancy_oak/fallen_oak entries dropped, see TreeDecoration's own KDoc).
            Identifier.vanilla("plains") to combine(TreeDecoration.single(TreeConfigs.OAK, 0, 0.05f, 1), PlainsDecoration),
            Identifier.vanilla("sunflower_plains") to combine(TreeDecoration.single(TreeConfigs.OAK, 0, 0.05f, 1), PlainsDecoration),
            // parity: OverworldBiomes.forest(birch=false)/addOtherBirchTrees: TREES_BIRCH_AND_OAK_LEAF_LITTER, oak default.
            Identifier.vanilla("forest") to TreeDecoration.single(TreeConfigs.OAK, 10, 0.1f, 1),
            // parity: OverworldBiomes.forest(birch=true, tall=false)/addBirchTrees: TREES_BIRCH, birch default.
            Identifier.vanilla("birch_forest") to TreeDecoration.single(TreeConfigs.BIRCH, 10, 0.1f, 1),
            // parity: OverworldBiomes.forest(birch=true, tall=true)/addTallBirchTrees: BIRCH_TALL, super_birch default.
            Identifier.vanilla("old_growth_birch_forest") to TreeDecoration.single(TreeConfigs.SUPER_BIRCH, 10, 0.1f, 1),
            // parity: OverworldBiomes.taiga/addTaigaTrees: TREES_TAIGA, 33.3% pine else spruce (fallen_spruce entry dropped).
            Identifier.vanilla("taiga") to TreeDecoration.choice(TreeConfigs.PINE, 0.33333334f, TreeConfigs.SPRUCE, 10, 0.1f, 1),
            // parity: OverworldBiomes.taiga(snowy=true)/addSnowyTrees: TREES_SNOWY, spruce default.
            Identifier.vanilla("snowy_taiga") to TreeDecoration.single(TreeConfigs.SPRUCE, 0, 0.1f, 1),
            // parity: OverworldBiomes.savanna/addSavannaTrees: TREES_SAVANNA, 80% acacia else oak (fallen_oak entry dropped).
            Identifier.vanilla("savanna") to TreeDecoration.choice(TreeConfigs.ACACIA, 0.8f, TreeConfigs.OAK, 1, 0.1f, 1),
            // parity: OverworldBiomes.savanna(plateau=true)/addSavannaTrees: same TREES_SAVANNA feature and species mix as plains savanna.
            Identifier.vanilla("savanna_plateau") to TreeDecoration.choice(TreeConfigs.ACACIA, 0.8f, TreeConfigs.OAK, 1, 0.1f, 1),
            // parity: OverworldBiomes.savanna(shattered=true)/addShatteredSavannaTrees: TREES_WINDSWEPT_SAVANNA, same species mix.
            Identifier.vanilla("windswept_savanna") to TreeDecoration.choice(TreeConfigs.ACACIA, 0.8f, TreeConfigs.OAK, 2, 0.1f, 1),
            // parity: OverworldBiomes.jungle/addJungleTrees: TREES_JUNGLE, jungle_tree default (fancy_oak/jungle_bush/mega entries dropped).
            Identifier.vanilla("jungle") to TreeDecoration.single(TreeConfigs.JUNGLE_TREE, 50, 0.1f, 1),
            // parity: OverworldBiomes.sparseJungle/addSparseJungleTrees: TREES_SPARSE_JUNGLE, jungle_tree default.
            Identifier.vanilla("sparse_jungle") to TreeDecoration.single(TreeConfigs.JUNGLE_TREE, 2, 0.1f, 1),
            // parity: OverworldBiomes.windsweptHills(moreTrees=false)/addMountainTrees: TREES_WINDSWEPT_HILLS, 66.6% spruce else oak.
            Identifier.vanilla("windswept_hills") to TreeDecoration.choice(TreeConfigs.SPRUCE, 0.666f, TreeConfigs.OAK, 0, 0.1f, 1),
            Identifier.vanilla("windswept_gravelly_hills") to TreeDecoration.choice(TreeConfigs.SPRUCE, 0.666f, TreeConfigs.OAK, 0, 0.1f, 1),
            // parity: OverworldBiomes.windsweptHills(moreTrees=true)/addMountainForestTrees: TREES_WINDSWEPT_FOREST, same species mix.
            Identifier.vanilla("windswept_forest") to TreeDecoration.choice(TreeConfigs.SPRUCE, 0.666f, TreeConfigs.OAK, 3, 0.1f, 1),
            // parity: OverworldBiomes.grove/addGroveTrees: TREES_GROVE, 33.3% pine else spruce (real "on snow" placement filter dropped).
            Identifier.vanilla("grove") to TreeDecoration.choice(TreeConfigs.PINE, 0.33333334f, TreeConfigs.SPRUCE, 10, 0.1f, 1),
            // parity: OverworldBiomes.badlands(wooded=true)/addBadlandsTrees: TREES_BADLANDS, oak default (oak_leaf_litter/fallen_oak simplified to plain oak).
            Identifier.vanilla("wooded_badlands") to combine(TreeDecoration.single(TreeConfigs.OAK, 5, 0.1f, 1), BadlandsDecoration),
            // parity: OverworldBiomes.badlands(wooded=false)/addBadlandGrass: patch_dead_bush_badlands (no trees for the plain/eroded variants).
            Identifier.vanilla("badlands") to BadlandsDecoration,
            Identifier.vanilla("eroded_badlands") to BadlandsDecoration,
            // parity: OverworldBiomes.swamp/addSwampVegetation: TREES_SWAMP, swamp_oak (a single feature, not a random selector).
            Identifier.vanilla("swamp") to TreeDecoration.single(TreeConfigs.SWAMP_OAK, 2, 0.1f, 1),
            // parity: OverworldBiomes.desert/addDesertVegetation+addDesertExtraVegetation: patch_cactus_desert, patch_dead_bush_2.
            Identifier.vanilla("desert") to DesertDecoration,
            // parity: OverworldBiomes.darkForest(isPaleGarden=false)/DARK_FOREST_VEGETATION: real RANDOM_SELECTOR,
            // huge mushrooms dropped (2.5%/5%), dark_oak 66.7%, fallen_birch dropped (0.25%), birch 20%, fallen_oak
            // dropped (1.25%), fancy_oak dropped (10%), oak default. CountPlacement(16), not countExtra.
            Identifier.vanilla("dark_forest") to
                TreeDecoration.sequence(
                    listOf(0.025f to null, 0.05f to null, 0.6666667f to TreeConfigs.DARK_OAK, 0.0025f to null, 0.2f to TreeConfigs.BIRCH, 0.0125f to null, 0.1f to null),
                    TreeConfigs.OAK,
                    16,
                ),
            // parity: OverworldBiomes.darkForest(isPaleGarden=true)/PALE_GARDEN_VEGETATION: 10% pale_oak_creaking
            // (same tree shape as the default, only the CreakingHeartDecorator differs, not ported), else pale_oak.
            // CountPlacement(16). Pale moss carpet/pale garden flowers not ported.
            Identifier.vanilla("pale_garden") to TreeDecoration.fixed(TreeConfigs.PALE_OAK, 16),
        )

    public fun forBiome(id: Identifier): BiomeVegetalDecoration? = byBiome[id]
}
