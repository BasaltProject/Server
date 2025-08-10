package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** parity: `BambooStalkBlock.randomTick` and `BambooSaplingBlock.randomTick`. */
public object BambooGrowth {
    /**
     * parity: `BambooStalkBlock.randomTick` and its `growBamboo`: one roll in three, light nine, and
     * a stalk that carries its leaves down as it climbs.
     */
    public fun bamboo(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["stage"] == "1") return
        if (random.nextInt(BAMBOO_ODDS) != 0) return
        if (world.state.getBlockState(x, y + 1, z) != Blocks.AIR.stateId) return
        if (WorldLight.rawBrightness(world, x, y + 1, z) < GROWTH_LIGHT) return

        var height = 1
        while (isBamboo(world, x, y - height, z)) height++
        if (height >= BAMBOO_MAX_HEIGHT) return

        val below = world.state.getBlockState(x, y - 1, z) ?: return
        val twoBelow = world.state.getBlockState(x, y - 2, z) ?: return
        val belowLeaves = BlockStateDefinitions.propertiesOf(below)["leaves"]
        var leaves = "none"
        if (!isBamboo(world, x, y - 1, z) || belowLeaves == "none") {
            leaves = "small"
        } else {
            leaves = "large"
            if (isBamboo(world, x, y - 2, z)) {
                shrink(world, x, y - 1, z, below, "small", changes)
                shrink(world, x, y - 2, z, twoBelow, "none", changes)
            }
        }
        val age = if (properties["age"] != "1" && !isBamboo(world, x, y - 2, z)) "0" else "1"
        val stage =
            if ((height < BAMBOO_TALL || random.nextFloat() >= BAMBOO_STAGE_CHANCE) &&
                height != 15
            ) {
                "0"
            } else {
                "1"
            }
        val grown =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla("bamboo"),
                mapOf("age" to age, "leaves" to leaves, "stage" to stage),
            ) ?: return
        set(world, x, y + 1, z, grown, changes)
    }

    /** parity: `BambooSaplingBlock.randomTick`, which puts a stalk above itself. */
    public fun bambooSapling(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextInt(BAMBOO_ODDS) != 0) return
        if (world.state.getBlockState(x, y + 1, z) != Blocks.AIR.stateId) return
        if (WorldLight.rawBrightness(world, x, y + 1, z) < GROWTH_LIGHT) return
        val stalk = Blocks.REGISTRY.idOf(Identifier.vanilla("bamboo")) ?: return
        set(world, x, y + 1, z, stalk, changes)
    }

    private fun shrink(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        leaves: String,
        changes: MutableList<BlockChange>,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val smaller = BlockStateDefinitions.stateIdOf(block, mapOf("leaves" to leaves), stateId) ?: return
        set(world, x, y, z, smaller, changes)
    }

    private fun isBamboo(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path == "bamboo"

    private const val BAMBOO_ODDS = 3
    private const val BAMBOO_MAX_HEIGHT = 16
    private const val BAMBOO_TALL = 11
    private const val BAMBOO_STAGE_CHANCE = 0.25f
}
