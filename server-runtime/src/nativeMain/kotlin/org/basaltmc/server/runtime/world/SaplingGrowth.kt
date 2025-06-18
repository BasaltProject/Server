package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfigs
import org.basaltmc.server.world.terrain.decoration.tree.TreeConfiguration
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature

/**
 * A sapling becoming a tree.
 *
 * parity: `SaplingBlock.advanceTree` over `TreeGrower.growTree`, using the very features worldgen
 * plants, so a grown tree has the same shape as one the world came with.
 *
 * Grown on a scratch chunk with the sapling at its middle and only then copied into the world: the
 * decoration features mask their X/Z with `and 15`, so a canopy reaching the next chunk would wrap
 * back into this one. Centred at local 8, nothing vanilla plants is wide enough to reach an edge.
 */
public object SaplingGrowth {
    /** parity: `SaplingBlock.randomTick`, which needs light nine above it and rolls one in seven. */
    public const val GROWTH_LIGHT: Int = 9
    public const val GROWTH_ODDS: Int = 7

    /** Where the scratch chunk puts the sapling, far enough from every edge for any canopy. */
    private const val CENTRE = 8

    /** How far above the sapling a tree may reach, which bounds what is copied back. */
    private const val HEIGHT = 32

    /** parity: `TreeGrower`, the pairs each sapling picks between and how often it picks the second. */
    private class Grower(
        val tree: TreeConfiguration,
        val secondary: TreeConfiguration? = null,
        val secondaryChance: Float = 0f,
    )

    private val GROWERS: Map<String, Grower> by lazy {
        mapOf(
            "oak_sapling" to Grower(TreeConfigs.OAK, TreeConfigs.FANCY_OAK, secondaryChance = 0.1f),
            "birch_sapling" to Grower(TreeConfigs.BIRCH),
            "spruce_sapling" to Grower(TreeConfigs.SPRUCE),
            "jungle_sapling" to Grower(TreeConfigs.JUNGLE_TREE),
            "acacia_sapling" to Grower(TreeConfigs.ACACIA),
            "dark_oak_sapling" to Grower(TreeConfigs.DARK_OAK),
            "pale_oak_sapling" to Grower(TreeConfigs.PALE_OAK),
            "cherry_sapling" to Grower(TreeConfigs.CHERRY),
            "mangrove_propagule" to Grower(TreeConfigs.MANGROVE),
        )
    }

    public fun isSapling(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path in GROWERS.keys

    /**
     * One sapling's turn: it grows a stage, or becomes a tree. Answers every block that changed.
     *
     * parity: `SaplingBlock.advanceTree`: stage zero only advances the stage, and a sapling that
     * has no room stays where it is rather than half a tree appearing.
     */
    public fun advance(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
    ): List<BlockChange> {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return emptyList()
        val grower = GROWERS[block.path] ?: return emptyList()
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["stage"] == "0") {
            val grown = BlockStateDefinitions.stateIdOf(block, mapOf("stage" to "1"), stateId) ?: return emptyList()
            if (!world.state.setBlockState(x, y, z, grown)) return emptyList()
            return listOf(BlockChange(x, y, z, grown))
        }
        val config =
            if (grower.secondary != null && random.nextFloat() < grower.secondaryChance) {
                grower.secondary
            } else {
                grower.tree
            }
        return plant(world, x, y, z, config, random)
    }

    /** Grows [config] on a scratch chunk and copies what it made into the world. */
    private fun plant(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        config: TreeConfiguration,
        random: LegacyRandom,
    ): List<BlockChange> {
        val bounds = world.state.chunkIfLoaded(x shr 4, z shr 4)?.verticalBounds ?: return emptyList()
        val originX = x - CENTRE
        val originZ = z - CENTRE
        val lowest = maxOf(bounds.minY, y - 1)
        val highest = minOf(bounds.maxY, y + HEIGHT)

        val scratch = Chunk(0, 0, bounds)
        val before = IntArray(16 * 16 * (highest - lowest + 1))
        var index = 0
        for (localX in 0..15) {
            for (localZ in 0..15) {
                for (level in lowest..highest) {
                    val stateId = world.state.getBlockState(originX + localX, level, originZ + localZ) ?: 0
                    scratch.setBlockStateId(localX, level, localZ, stateId)
                    before[index++] = stateId
                }
            }
        }
        // The sapling itself goes first, as vanilla clears it before placing.
        scratch.setBlockStateId(CENTRE, y, CENTRE, Blocks.AIR.stateId)

        val worldgen = WorldgenRandom(LegacyRandom(random.nextLong()))
        if (!TreeFeature.place(scratch, worldgen, CENTRE, y, CENTRE, config)) return emptyList()

        val changes = mutableListOf<BlockChange>()
        index = 0
        for (localX in 0..15) {
            for (localZ in 0..15) {
                for (level in lowest..highest) {
                    val after = scratch.getBlockStateId(localX, level, localZ)
                    val was = before[index++]
                    if (after == was) continue
                    val worldX = originX + localX
                    val worldZ = originZ + localZ
                    if (!world.state.setBlockState(worldX, level, worldZ, after)) continue
                    changes += BlockChange(worldX, level, worldZ, after)
                }
            }
        }
        return changes
    }
}
