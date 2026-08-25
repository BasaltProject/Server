package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** parity: `GrowingPlantHeadBlock.randomTick`, kelp, cave vines, twisting and weeping vines. */
public object ClimbingPlantGrowth {
    /**
     * parity: `GrowingPlantHeadBlock.randomTick`: kelp, cave vines and the nether vines all climb
     * their own way at their own odds until they reach age twenty-five.
     */
    public fun growingPlant(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        upwards: Boolean,
        chance: Double,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age >= PLANT_MAX_AGE) return
        if (random.nextDouble() >= chance) return
        val nextY = if (upwards) y + 1 else y - 1
        val ahead = world.state.getBlockState(x, nextY, z) ?: return
        // parity: `canGrowInto`, which for kelp is water and for the rest is air.
        val room = if (block.path == "kelp") BlockSupport.holdsWater(ahead) else ahead == Blocks.AIR.stateId
        if (!room) return
        val grown =
            BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
        set(world, x, nextY, z, grown, changes)
        val body = BODY_OF[block.path] ?: return
        Blocks.REGISTRY.idOf(Identifier.vanilla(body))?.let { set(world, x, y, z, it, changes) }
    }

    /** What each climbing plant leaves behind it. parity: `GrowingPlantHeadBlock.getBodyBlock`. */
    private val BODY_OF =
        mapOf(
            "kelp" to "kelp_plant",
            "cave_vines" to "cave_vines_plant",
            "twisting_vines" to "twisting_vines_plant",
            "weeping_vines" to "weeping_vines_plant",
        )

    private const val PLANT_MAX_AGE = 25
}
