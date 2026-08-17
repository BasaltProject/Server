package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * Potent sulfur under water: what it turns into, and the clock a periodic geyser keeps.
 *
 * parity: `PotentSulfurBlock.validBlockState` for the state, and
 * `PotentSulfurBlockEntity.SERVER_WAITING_COUNTDOWN_TICKER` for the wait between erupting and lying
 * dormant. Vanilla counts that wait down twenty ticks at a time with nothing to show for it; here
 * the turn it lands on is scheduled instead, which the chunk already knows how to keep.
 *
 * What the plume does to whoever swims into it waits for entities.
 */
public object Geysers {
    public val POTENT_SULFUR: Identifier = Identifier.vanilla("potent_sulfur")

    /** parity: `PotentSulfurBlock.ALLOWED_WATER_BLOCKS_ABOVE`. */
    private const val ALLOWED_WATER_BLOCKS_ABOVE = 4

    /** parity: `PotentSulfurBlockEntity.GEYSER_SALT`. */
    private const val GEYSER_SALT = -904011478L

    /** The countdown ticker only looks every twenty ticks, so the wait is counted in those. */
    private const val COUNTDOWN_STEP = 20

    /**
     * parity: `PotentSulfurBlock.validBlockState`. Answers the state this block should be in, or
     * `null` when it is already in it.
     */
    public fun validState(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        val current = BlockStateDefinitions.propertiesOf(stateId)["potent_sulfur_state"] ?: return null
        val wanted = wantedState(world, x, y, z, current)
        if (wanted == current) return null
        val turned =
            BlockStateDefinitions.stateIdOf(POTENT_SULFUR, mapOf("potent_sulfur_state" to wanted), stateId)
                ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        // Entering a wait starts one; leaving it means the clock is somebody else's problem.
        if (wanted == "dormant" || wanted == "erupting") scheduleTurn(world, x, y, z, wanted)
        return turned
    }

    private fun wantedState(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        current: String,
    ): String {
        if (!isWaterSource(world, x, y + 1, z)) return "dry"
        val below = world.state.getBlockState(x, y - 1, z) ?: return "dry"
        // parity: `isSourceIfFluid`, which lets flowing lava out of it.
        val stillOrSolid = !isFlowing(below)
        if (BlockConnections.erupsContinuously(below) && stillOrSolid) return "continuous"
        if (BlockConnections.eruptsPeriodically(below) && stillOrSolid) {
            return if (current == "erupting") "erupting" else "dormant"
        }
        return "wet"
    }

    /**
     * parity: `SERVER_WAITING_COUNTDOWN_TICKER`, whose countdown depends on how deep the water is
     * and on a random forked from the seed at this very position, so two geysers side by side keep
     * their own rhythm and the same world always keeps the same one.
     */
    public fun scheduleTurn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        state: String,
    ) {
        val sourceY = gasSourceY(world, x, y, z) ?: return
        val waterBlocks = sourceY - y - 1
        val random = XoroshiroRandom(world.metadata.seed xor GEYSER_SALT).forkPositional().at(x, y, z)
        val steps =
            if (state == "dormant") {
                10 * (waterBlocks - 1) + random.nextIntBetweenInclusive(15, 30)
            } else {
                random.nextInt()
                waterBlocks - 1 + random.nextIntBetweenInclusive(1, 2)
            }
        if (steps <= 0) return
        // The ticker only acts on a twenty-tick boundary, so the turn lands on one too.
        val toBoundary = COUNTDOWN_STEP - (world.gameTime % COUNTDOWN_STEP).toInt()
        world.state.scheduleBlockTick(x, y, z, POTENT_SULFUR, toBoundary + (steps - 1) * COUNTDOWN_STEP)
    }

    /** parity: the flip at the end of that countdown: dormant becomes erupting, and erupting rests. */
    public fun tick(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        if (BlockStateDefinitions.blockOf(stateId)?.path != "potent_sulfur") return
        val state = BlockStateDefinitions.propertiesOf(stateId)["potent_sulfur_state"] ?: return
        if (state != "dormant" && state != "erupting") return
        if (gasSourceY(world, x, y, z) == null) return
        val turnedTo = if (state == "dormant") "erupting" else "dormant"
        val turned =
            BlockStateDefinitions.stateIdOf(POTENT_SULFUR, mapOf("potent_sulfur_state" to turnedTo), stateId) ?: return
        if (!world.state.setBlockState(x, y, z, turned)) return
        changes += BlockChange(x, y, z, turned)
        scheduleTurn(world, x, y, z, turnedTo)
    }

    /**
     * parity: `findNoxiousGasSourceBlock`: up through the water over the block, at most four of it,
     * to the open block on top. Null when something caps it, which is a geyser that does nothing.
     */
    private fun gasSourceY(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val maxY = y + ALLOWED_WATER_BLOCKS_ABOVE + 1
        var above = y + 1
        while (above <= maxY) {
            val stateId = world.state.getBlockState(x, above, z) ?: return null
            val water = isWaterSource(world, x, above, z)
            if (!water || (!isWater(stateId) && !isPassable(stateId))) {
                return if (stateId == Blocks.AIR.stateId || isPassable(stateId)) above else null
            }
            above++
        }
        return null
    }

    /** parity: `isGeyserPassableBlock`: air, water, or anything the plume can rise through. */
    private fun isPassable(stateId: Int): Boolean =
        stateId == Blocks.AIR.stateId || isWater(stateId) || BlockShapes.isEmptyShape(stateId)

    private fun isWater(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path == "water"

    private fun isWaterSource(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        if (isWater(stateId)) return BlockStateDefinitions.propertiesOf(stateId)["level"] == "0"
        return BlockStateDefinitions.propertiesOf(stateId)["waterlogged"] == "true"
    }

    /** Flowing lava or water, which vanilla will not let drive a geyser. */
    private fun isFlowing(stateId: Int): Boolean {
        val level = BlockStateDefinitions.propertiesOf(stateId)["level"] ?: return false
        return level != "0"
    }
}
