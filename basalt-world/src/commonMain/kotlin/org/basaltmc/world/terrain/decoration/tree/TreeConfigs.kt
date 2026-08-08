package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.terrain.decoration.IntProvider

/**
 * Real per-tree [TreeConfiguration]s, ported verbatim from the real jar's own decompiled
 * `net.minecraft.data.worldgen.features.TreeFeatures` (`createOak`/`createBirch`/
 * `createSuperBirch`/`createJungleTree`/`cherry`, and the inline `FANCY_OAK`/`SPRUCE`/`PINE`/`ACACIA` registrations).
 * Only the configs this project's currently-ported [org.basaltmc.world.terrain.decoration.tree.TrunkPlacer]/
 * [org.basaltmc.world.terrain.decoration.tree.FoliagePlacer] types can drive are included: no
 * azalea yet.
 */
public object TreeConfigs {
    private fun state(name: String): Int = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

    private val OAK_LOG = state("oak_log")
    private val OAK_LEAVES = state("oak_leaves")
    private val BIRCH_LOG = state("birch_log")
    private val BIRCH_LEAVES = state("birch_leaves")
    private val SPRUCE_LOG = state("spruce_log")
    private val SPRUCE_LEAVES = state("spruce_leaves")
    private val JUNGLE_LOG = state("jungle_log")
    private val JUNGLE_LEAVES = state("jungle_leaves")
    private val ACACIA_LOG = state("acacia_log")
    private val ACACIA_LEAVES = state("acacia_leaves")
    private val DARK_OAK_LOG = state("dark_oak_log")
    private val DARK_OAK_LEAVES = state("dark_oak_leaves")
    private val PALE_OAK_LOG = state("pale_oak_log")
    private val PALE_OAK_LEAVES = state("pale_oak_leaves")
    private val MANGROVE_LOG = state("mangrove_log")
    private val MANGROVE_LEAVES = state("mangrove_leaves")
    private val CHERRY_LOG = state("cherry_log")
    private val CHERRY_LEAVES = state("cherry_leaves")

    // parity: TreeFeatures.createOak/createStraightBlobTree(OAK_LOG, OAK_LEAVES, 4, 2, 0, 2).
    public val OAK: TreeConfiguration =
        TreeConfiguration(OAK_LOG, StraightTrunkPlacer(4, 2, 0), OAK_LEAVES, BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

    // parity: TreeFeatures.createStraightBlobTree(OAK_LOG, OAK_LEAVES, 5, 3, 0, 3) (swamp_oak).
    public val SWAMP_OAK: TreeConfiguration =
        TreeConfiguration(
            OAK_LOG,
            StraightTrunkPlacer(5, 3, 0),
            OAK_LEAVES,
            BlobFoliagePlacer(IntProvider.of(3), IntProvider.of(0), 3),
            FeatureSizeTwoLayers(1, 0, 1),
            decorators = listOf(TreeDecoratorConfiguration.LeafVines(0.25f)),
        )

    // parity: TreeFeatures.createBirch/createStraightBlobTree(BIRCH_LOG, BIRCH_LEAVES, 5, 2, 0, 2).
    public val BIRCH: TreeConfiguration =
        TreeConfiguration(BIRCH_LOG, StraightTrunkPlacer(5, 2, 0), BIRCH_LEAVES, BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

    // parity: TreeFeatures.createSuperBirch/createStraightBlobTree(BIRCH_LOG, BIRCH_LEAVES, 5, 2, 6, 2).
    public val SUPER_BIRCH: TreeConfiguration =
        TreeConfiguration(BIRCH_LOG, StraightTrunkPlacer(5, 2, 6), BIRCH_LEAVES, BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

    // parity: TreeFeatures.FANCY_OAK registration.
    public val FANCY_OAK: TreeConfiguration =
        TreeConfiguration(
            OAK_LOG,
            FancyTrunkPlacer(3, 11, 0),
            OAK_LEAVES,
            FancyFoliagePlacer(IntProvider.of(2), IntProvider.of(4), 4),
            FeatureSizeTwoLayers(0, 0, 0),
        )

    // parity: TreeFeatures.createJungleTree/createStraightBlobTree(JUNGLE_LOG, JUNGLE_LEAVES, 4, 8, 0, 2).
    public val JUNGLE_TREE: TreeConfiguration =
        TreeConfiguration(
            JUNGLE_LOG,
            StraightTrunkPlacer(4, 8, 0),
            JUNGLE_LEAVES,
            BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3),
            FeatureSizeTwoLayers(1, 0, 1),
            decorators =
                listOf(
                    TreeDecoratorConfiguration.Cocoa(0.2f),
                    TreeDecoratorConfiguration.TrunkVines,
                    TreeDecoratorConfiguration.LeafVines(0.25f),
                ),
        )

    // parity: TreeFeatures.JUNGLE_BUSH registration.
    public val JUNGLE_BUSH: TreeConfiguration =
        TreeConfiguration(
            JUNGLE_LOG,
            StraightTrunkPlacer(1, 0, 0),
            OAK_LEAVES,
            BushFoliagePlacer(IntProvider.of(2), IntProvider.of(1), 2),
            FeatureSizeTwoLayers(0, 0, 0),
        )

    // parity: TreeFeatures.MEGA_JUNGLE_TREE registration, including trunk and leaf vines.
    public val MEGA_JUNGLE: TreeConfiguration =
        TreeConfiguration(
            JUNGLE_LOG,
            MegaJungleTrunkPlacer(10, 2, 19),
            JUNGLE_LEAVES,
            MegaJungleFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 2),
            FeatureSizeTwoLayers(1, 1, 2),
            decorators =
                listOf(
                    TreeDecoratorConfiguration.TrunkVines,
                    TreeDecoratorConfiguration.LeafVines(0.25f),
                ),
        )

    // parity: TreeFeatures.SPRUCE registration.
    public val SPRUCE: TreeConfiguration =
        TreeConfiguration(
            SPRUCE_LOG,
            StraightTrunkPlacer(5, 2, 1),
            SPRUCE_LEAVES,
            SpruceFoliagePlacer(IntProvider.uniform(2, 3), IntProvider.uniform(0, 2), IntProvider.uniform(1, 2)),
            FeatureSizeTwoLayers(2, 0, 2),
        )

    // parity: TreeFeatures.PINE registration.
    public val PINE: TreeConfiguration =
        TreeConfiguration(
            SPRUCE_LOG,
            StraightTrunkPlacer(6, 4, 0),
            SPRUCE_LEAVES,
            PineFoliagePlacer(IntProvider.of(1), IntProvider.of(1), IntProvider.uniform(3, 4)),
            FeatureSizeTwoLayers(2, 0, 2),
        )

    // parity: TreeFeatures.ACACIA registration.
    public val ACACIA: TreeConfiguration =
        TreeConfiguration(
            ACACIA_LOG,
            ForkingTrunkPlacer(5, 2, 2),
            ACACIA_LEAVES,
            AcaciaFoliagePlacer(IntProvider.of(2), IntProvider.of(0)),
            FeatureSizeTwoLayers(1, 0, 2),
        )

    // parity: TreeFeatures.createDarkOak/DarkOakTrunkPlacer(6, 2, 1)/DarkOakFoliagePlacer(0, 0)/ThreeLayersFeatureSize(1, 1, 0, 1, 2).
    public val DARK_OAK: TreeConfiguration =
        TreeConfiguration(
            DARK_OAK_LOG,
            DarkOakTrunkPlacer(6, 2, 1),
            DARK_OAK_LEAVES,
            DarkOakFoliagePlacer(IntProvider.of(0), IntProvider.of(0)),
            FeatureSizeThreeLayers(1, 1, 0, 1, 2),
        )

    // parity: TreeFeatures.PALE_OAK registration: same DarkOakTrunkPlacer/DarkOakFoliagePlacer shape, pale_oak blocks.
    public val PALE_OAK: TreeConfiguration =
        TreeConfiguration(
            PALE_OAK_LOG,
            DarkOakTrunkPlacer(6, 2, 1),
            PALE_OAK_LEAVES,
            DarkOakFoliagePlacer(IntProvider.of(0), IntProvider.of(0)),
            FeatureSizeThreeLayers(1, 1, 0, 1, 2),
            decorators = listOf(TreeDecoratorConfiguration.PaleMoss(0.15f, 0.4f, 0.8f)),
        )

    // parity: TreeFeatures.PALE_OAK_CREAKING, pale moss followed by a guaranteed heart attempt.
    public val PALE_OAK_CREAKING: TreeConfiguration =
        TreeConfiguration(
            PALE_OAK_LOG,
            DarkOakTrunkPlacer(6, 2, 1),
            PALE_OAK_LEAVES,
            DarkOakFoliagePlacer(IntProvider.of(0), IntProvider.of(0)),
            FeatureSizeThreeLayers(1, 1, 0, 1, 2),
            decorators =
                listOf(
                    TreeDecoratorConfiguration.PaleMoss(0.15f, 0.4f, 0.8f),
                    TreeDecoratorConfiguration.CreakingHeart(1.0f),
                ),
        )

    // parity: TreeFeatures.MEGA_SPRUCE registration.
    public val MEGA_SPRUCE: TreeConfiguration =
        TreeConfiguration(
            SPRUCE_LOG,
            GiantTrunkPlacer(13, 2, 14),
            SPRUCE_LEAVES,
            MegaPineFoliagePlacer(IntProvider.of(0), IntProvider.of(0), IntProvider.uniform(13, 17)),
            FeatureSizeTwoLayers(1, 1, 2),
            decorators = listOf(TreeDecoratorConfiguration.AlterGroundToPodzol),
        )

    // parity: TreeFeatures.MEGA_PINE registration.
    public val MEGA_PINE: TreeConfiguration =
        TreeConfiguration(
            SPRUCE_LOG,
            GiantTrunkPlacer(13, 2, 14),
            SPRUCE_LEAVES,
            MegaPineFoliagePlacer(IntProvider.of(0), IntProvider.of(0), IntProvider.uniform(3, 7)),
            FeatureSizeTwoLayers(1, 1, 2),
            decorators = listOf(TreeDecoratorConfiguration.AlterGroundToPodzol),
        )

    // parity: TreeFeatures.MANGROVE registration (the small variant; real MangroveRootPlacer with
    // trunk_offset_y uniform(1, 3)).
    public val MANGROVE: TreeConfiguration =
        TreeConfiguration(
            MANGROVE_LOG,
            UpwardsBranchingTrunkPlacer(2, 1, 4, IntProvider.uniform(1, 4), 0.5f, IntProvider.uniform(0, 1)),
            MANGROVE_LEAVES,
            RandomSpreadFoliagePlacer(IntProvider.of(3), IntProvider.of(0), IntProvider.of(2), 70),
            FeatureSizeTwoLayers(2, 0, 2),
            MangroveRootPlacer(IntProvider.uniform(1, 3)),
            decorators =
                listOf(
                    TreeDecoratorConfiguration.LeafVines(0.125f),
                    TreeDecoratorConfiguration.MangrovePropagules(0.14f, 1, 0, 2),
                    TreeDecoratorConfiguration.Beehive(0.01f),
                ),
        )

    // parity: TreeFeatures.TALL_MANGROVE registration (trunk_offset_y uniform(3, 7)).
    public val TALL_MANGROVE: TreeConfiguration =
        TreeConfiguration(
            MANGROVE_LOG,
            UpwardsBranchingTrunkPlacer(4, 1, 9, IntProvider.uniform(1, 6), 0.5f, IntProvider.uniform(0, 1)),
            MANGROVE_LEAVES,
            RandomSpreadFoliagePlacer(IntProvider.of(3), IntProvider.of(0), IntProvider.of(2), 70),
            FeatureSizeTwoLayers(3, 0, 2),
            MangroveRootPlacer(IntProvider.uniform(3, 7)),
            decorators =
                listOf(
                    TreeDecoratorConfiguration.LeafVines(0.125f),
                    TreeDecoratorConfiguration.MangrovePropagules(0.14f, 1, 0, 2),
                    TreeDecoratorConfiguration.Beehive(0.01f),
                ),
        )

    // parity: TreeFeatures.cherry (CHERRY registration). branchCount is the real WeightedListInt
    // over ConstantInt(1/2/3), one weight each.
    public val CHERRY: TreeConfiguration =
        TreeConfiguration(
            CHERRY_LOG,
            CherryTrunkPlacer(
                7,
                1,
                0,
                IntProvider.Weighted(1 to 1, 2 to 1, 3 to 1),
                IntProvider.uniform(2, 4),
                IntProvider.Uniform(-4, -3),
                IntProvider.uniform(-1, 0),
            ),
            CHERRY_LEAVES,
            CherryFoliagePlacer(IntProvider.of(4), IntProvider.of(0), IntProvider.of(5), 0.25f, 0.5f, 0.16666667f, 0.33333334f),
            FeatureSizeTwoLayers(1, 0, 2),
        )

    private val LEAF_LITTER_DECORATORS =
        listOf(
            TreeDecoratorConfiguration.GroundLeafLitter(96, 4, 2, 3),
            TreeDecoratorConfiguration.GroundLeafLitter(150, 2, 2, 4),
        )

    // parity: sparseLeafLitter followed by thickLeafLitter on the dedicated 26.2 tree configs.
    public val OAK_LEAF_LITTER: TreeConfiguration = withDecorators(OAK, LEAF_LITTER_DECORATORS)
    public val BIRCH_LEAF_LITTER: TreeConfiguration = withDecorators(BIRCH, LEAF_LITTER_DECORATORS)
    public val FANCY_OAK_LEAF_LITTER: TreeConfiguration = withDecorators(FANCY_OAK, LEAF_LITTER_DECORATORS)
    public val DARK_OAK_LEAF_LITTER: TreeConfiguration = withDecorators(DARK_OAK, LEAF_LITTER_DECORATORS)

    public val OAK_BEES_0002_LEAF_LITTER: TreeConfiguration =
        withDecorators(OAK, listOf(TreeDecoratorConfiguration.Beehive(0.002f)) + LEAF_LITTER_DECORATORS)
    public val BIRCH_BEES_0002_LEAF_LITTER: TreeConfiguration =
        withDecorators(BIRCH, listOf(TreeDecoratorConfiguration.Beehive(0.002f)) + LEAF_LITTER_DECORATORS)
    public val FANCY_OAK_BEES_0002_LEAF_LITTER: TreeConfiguration =
        withDecorators(FANCY_OAK, listOf(TreeDecoratorConfiguration.Beehive(0.002f)) + LEAF_LITTER_DECORATORS)

    public val OAK_BEES_002: TreeConfiguration = withDecorators(OAK, listOf(TreeDecoratorConfiguration.Beehive(0.02f)))
    public val BIRCH_BEES_002: TreeConfiguration = withDecorators(BIRCH, listOf(TreeDecoratorConfiguration.Beehive(0.02f)))
    public val FANCY_OAK_BEES_002: TreeConfiguration = withDecorators(FANCY_OAK, listOf(TreeDecoratorConfiguration.Beehive(0.02f)))
    public val OAK_BEES_005: TreeConfiguration = withDecorators(OAK, listOf(TreeDecoratorConfiguration.Beehive(0.05f)))
    public val FANCY_OAK_BEES_005: TreeConfiguration = withDecorators(FANCY_OAK, listOf(TreeDecoratorConfiguration.Beehive(0.05f)))
    public val BIRCH_BEES_0002: TreeConfiguration = withDecorators(BIRCH, listOf(TreeDecoratorConfiguration.Beehive(0.002f)))
    public val SUPER_BIRCH_BEES_0002: TreeConfiguration = withDecorators(SUPER_BIRCH, listOf(TreeDecoratorConfiguration.Beehive(0.002f)))
    public val FANCY_OAK_BEES: TreeConfiguration = withDecorators(FANCY_OAK, listOf(TreeDecoratorConfiguration.Beehive(1.0f)))
    public val SUPER_BIRCH_BEES: TreeConfiguration = withDecorators(SUPER_BIRCH, listOf(TreeDecoratorConfiguration.Beehive(1.0f)))
    public val CHERRY_BEES_005: TreeConfiguration = withDecorators(CHERRY, listOf(TreeDecoratorConfiguration.Beehive(0.05f)))

    private fun withDecorators(
        base: TreeConfiguration,
        decorators: List<TreeDecoratorConfiguration>,
    ): TreeConfiguration =
        TreeConfiguration(
            base.trunkStateId,
            base.trunkPlacer,
            base.foliageStateId,
            base.foliagePlacer,
            base.minimumSize,
            base.rootPlacer,
            decorators,
        )

}
