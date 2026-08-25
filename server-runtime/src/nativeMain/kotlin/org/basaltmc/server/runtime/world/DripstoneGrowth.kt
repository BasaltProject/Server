package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** parity: `SpeleothemBlock.randomTick`, dripstone spikes growing tips and drips. */
public object DripstoneGrowth {
    /**
     * parity: `SpeleothemBlock.randomTick` over `growStalactiteOrStalagmiteIfPossible`: a stalactite
     * hanging under dripstone grows a tip down, or drips a stalagmite up from the floor below it.
     *
     * What is not here is the other half of `PointedDripstoneBlock.randomTick`, which fills a
     * cauldron under it, which waits for cauldrons to hold anything.
     */
    public fun dripstone(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (random.nextFloat() >= DRIPSTONE_CHANCE) return
        val spike = BlockStateDefinitions.blockOf(stateId)?.path ?: return
        val grownOn = GROWS_ON[spike] ?: return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        if (properties["vertical_direction"] != "down") return
        // parity: `isStalactiteStartPos`, the topmost block of the stalactite.
        if (isSpeleothem(world, x, y + 1, z, spike)) return
        // parity: `canGrow`, which asks for the block it hangs from.
        if (BlockStateDefinitions.blockOf(world.state.getBlockState(x, y + 1, z) ?: 0)?.path != grownOn) return

        var tipY = y
        var found = false
        repeat(REACH_OF[spike] ?: DRIPSTONE_REACH) {
            val here = world.state.getBlockState(x, tipY, z) ?: return
            val thickness = BlockStateDefinitions.propertiesOf(here)["thickness"]
            if (thickness == "tip") {
                found = true
                return@repeat
            }
            if (!isSpeleothem(world, x, tipY - 1, z, spike)) return
            tipY--
        }
        if (!found) return
        val tip = world.state.getBlockState(x, tipY, z) ?: return
        val tipProperties = BlockStateDefinitions.propertiesOf(tip)
        // parity: `isFreeHangingStalactite`, which will not grow from a waterlogged tip.
        if (tipProperties["waterlogged"] == "true") return
        val below = world.state.getBlockState(x, tipY - 1, z) ?: return
        if (below != Blocks.AIR.stateId) return

        if (random.nextBoolean()) {
            placeTip(world, x, tipY - 1, z, "down", spike, changes)
            return
        }
        // parity: `growStalagmiteBelow`, ten blocks down for a floor to grow up from.
        var floorY = tipY - 1
        repeat(DRIPSTONE_DROP) {
            val here = world.state.getBlockState(x, floorY, z) ?: return
            if (BlockSupport.holdsWater(here)) return
            if (here != Blocks.AIR.stateId) {
                if (BlockShapes.isFaceSturdy(here, org.basaltmc.server.registry.BlockFace.UP)) {
                    placeTip(world, x, floorY + 1, z, "up", spike, changes)
                }
                return
            }
            floorY--
        }
    }

    private fun isSpeleothem(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        spike: String,
    ): Boolean = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path == spike

    private fun placeTip(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        direction: String,
        spike: String,
        changes: MutableList<BlockChange>,
    ) {
        val tip =
            BlockStateDefinitions.stateIdOf(
                Identifier.vanilla(spike),
                mapOf("vertical_direction" to direction, "thickness" to "tip", "waterlogged" to "false"),
            ) ?: return
        set(world, x, y, z, tip, changes)
    }

    /** parity: `SpeleothemBlock.randomTick`'s own chance, its reach and its drop. */
    private const val DRIPSTONE_CHANCE = 0.011377778f
    private const val DRIPSTONE_REACH = 7

    /** parity: each `SpeleothemBlock`'s `blockToGrowOn` and `getMaxGrowthLength`. */
    private val GROWS_ON = mapOf("pointed_dripstone" to "dripstone_block", "sulfur_spike" to "sulfur")
    private val REACH_OF = mapOf("pointed_dripstone" to 7, "sulfur_spike" to 2)
    private const val DRIPSTONE_DROP = 10
}
