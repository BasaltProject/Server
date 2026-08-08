package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.terrain.decoration.IntProvider

/**
 * Real per-tree [TreeConfiguration]s, ported verbatim from the real jar's own decompiled
 * `net.minecraft.data.worldgen.features.TreeFeatures` (`createOak`/`createBirch`/
 * `createSuperBirch`/`createJungleTree`, and the inline `SPRUCE`/`PINE`/`ACACIA` registrations).
 * Only the configs this project's currently-ported [org.basaltmc.world.terrain.decoration.tree.TrunkPlacer]/
 * [org.basaltmc.world.terrain.decoration.tree.FoliagePlacer] types can drive are included: no
 * fancy oak, mega/giant trunks, cherry, mangrove or azalea yet.
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

    // parity: TreeFeatures.createOak/createStraightBlobTree(OAK_LOG, OAK_LEAVES, 4, 2, 0, 2).
    public val OAK: TreeConfiguration =
        TreeConfiguration(OAK_LOG, StraightTrunkPlacer(4, 2, 0), OAK_LEAVES, BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

    // parity: TreeFeatures.createStraightBlobTree(OAK_LOG, OAK_LEAVES, 5, 3, 0, 3) (swamp_oak).
    public val SWAMP_OAK: TreeConfiguration =
        TreeConfiguration(OAK_LOG, StraightTrunkPlacer(5, 3, 0), OAK_LEAVES, BlobFoliagePlacer(IntProvider.of(3), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

    // parity: TreeFeatures.createBirch/createStraightBlobTree(BIRCH_LOG, BIRCH_LEAVES, 5, 2, 0, 2).
    public val BIRCH: TreeConfiguration =
        TreeConfiguration(BIRCH_LOG, StraightTrunkPlacer(5, 2, 0), BIRCH_LEAVES, BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

    // parity: TreeFeatures.createSuperBirch/createStraightBlobTree(BIRCH_LOG, BIRCH_LEAVES, 5, 2, 6, 2).
    public val SUPER_BIRCH: TreeConfiguration =
        TreeConfiguration(BIRCH_LOG, StraightTrunkPlacer(5, 2, 6), BIRCH_LEAVES, BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

    // parity: TreeFeatures.createJungleTree/createStraightBlobTree(JUNGLE_LOG, JUNGLE_LEAVES, 4, 8, 0, 2).
    public val JUNGLE_TREE: TreeConfiguration =
        TreeConfiguration(JUNGLE_LOG, StraightTrunkPlacer(4, 8, 0), JUNGLE_LEAVES, BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 3), FeatureSizeTwoLayers(1, 0, 1))

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
        )
}
