package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks

/**
 * What a block change does to the blocks around it.
 *
 * parity: `Level.updateNeighborsAt` reaching each neighbour's `neighborChanged`, where a block that
 * can no longer stand breaks and drops.
 *
 * [BlockSupport] decides what breaks. A state whose rule this build does not model is left alone:
 * a block staying up when vanilla would drop it is far cheaper than one vanishing under a player.
 */
public object NeighbourUpdates {
    /**
     * How far one change is allowed to cascade. A tower of torches comes down in one go and this is
     * never reached; the bound is there so a rule that somehow contradicts itself cannot spin.
     */
    public const val MAX_CASCADE: Int = 1024

    /**
     * Breaks whatever [x], [y], [z] stopped holding up, and whatever that in turn stopped holding.
     *
     * [onBroken] is given each block as it goes, with the state it had, so the caller can drop its
     * loot and tell the clients: this knows about neither.
     */
    public fun afterChange(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        onBroken: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ) {
        val pending = ArrayDeque<Int>(INITIAL_QUEUE)
        push(pending, x, y, z)
        var examined = 0
        while (pending.isNotEmpty() && examined < MAX_CASCADE) {
            val px = pending.removeFirst()
            val py = pending.removeFirst()
            val pz = pending.removeFirst()
            examined += DIRECTIONS
            visit(world, pending, px, py - 1, pz, onBroken)
            visit(world, pending, px, py + 1, pz, onBroken)
            visit(world, pending, px, py, pz - 1, onBroken)
            visit(world, pending, px, py, pz + 1, onBroken)
            visit(world, pending, px - 1, py, pz, onBroken)
            visit(world, pending, px + 1, py, pz, onBroken)
        }
    }

    /** Breaks the block there if nothing holds it now, and follows on from whatever it held. */
    private fun visit(
        world: LoadedWorld,
        pending: ArrayDeque<Int>,
        x: Int,
        y: Int,
        z: Int,
        onBroken: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ) {
        if (breakIfUnsupported(world, x, y, z, onBroken)) push(pending, x, y, z)
    }

    /** Whether the block at these coordinates would stay where it is. */
    public fun isSupported(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int = world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId,
    ): Boolean {
        if (!BlockSupport.needsSupport(stateId) || !BlockSupport.isModelled(stateId)) return true
        val down = stateAt(world, x, y - 1, z)
        val up = stateAt(world, x, y + 1, z)
        val north = stateAt(world, x, y, z - 1)
        val south = stateAt(world, x, y, z + 1)
        val west = stateAt(world, x - 1, y, z)
        val east = stateAt(world, x + 1, y, z)
        return BlockSupport.canSurvive(stateId, down, up, north, south, west, east, wet = waterAbout(world, x, y, z))
    }

    private fun breakIfUnsupported(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        onBroken: (stateId: Int, x: Int, y: Int, z: Int) -> Unit,
    ): Boolean {
        if (y < world.minY || y > world.maxY) return false
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        if (stateId == Blocks.AIR.stateId) return false
        if (isSupported(world, x, y, z, stateId)) return false
        world.state.setBlockState(x, y, z, Blocks.AIR.stateId)
        onBroken(stateId, x, y, z)
        return true
    }

    /**
     * Whether there is water at this position, beside it, or beside the block under it.
     *
     * Sugar cane asks about the last of those and kelp about the first, and neither can be answered
     * from the block alone. Asking generously is deliberate: it can only keep a block standing.
     */
    private fun waterAbout(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        if (BlockSupport.holdsWater(stateAt(world, x, y, z))) return true
        for (level in intArrayOf(y, y - 1)) {
            if (BlockSupport.holdsWater(stateAt(world, x - 1, level, z))) return true
            if (BlockSupport.holdsWater(stateAt(world, x + 1, level, z))) return true
            if (BlockSupport.holdsWater(stateAt(world, x, level, z - 1))) return true
            if (BlockSupport.holdsWater(stateAt(world, x, level, z + 1))) return true
        }
        return false
    }

    private fun stateAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int = world.state.getBlockState(x, y, z) ?: Blocks.AIR.stateId

    private fun push(
        queue: ArrayDeque<Int>,
        x: Int,
        y: Int,
        z: Int,
    ) {
        queue.addLast(x)
        queue.addLast(y)
        queue.addLast(z)
    }

    private const val INITIAL_QUEUE = 24
    private const val DIRECTIONS = 6
}
