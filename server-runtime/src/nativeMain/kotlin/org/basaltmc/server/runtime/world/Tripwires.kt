package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier

/**
 * The string on the floor and the hooks it runs between.
 *
 * parity: `TripWireBlock.checkPressed` for the string, `TripWireHookBlock.calculateState` for the
 * pair of hooks. The hook is the part that gives off redstone, and it only does so when the line
 * between the two is unbroken: a string with nothing at both ends powers nothing.
 */
public object Tripwires {
    public val WIRE: Identifier = Identifier.vanilla("tripwire")
    public val HOOK: Identifier = Identifier.vanilla("tripwire_hook")

    /** parity: `TripWireBlock.RECHECK_PERIOD`, how long a pressed wire waits before looking again. */
    public const val RECHECK_TICKS: Int = 10

    /** parity: `calculateState`'s own 42, the longest a line of string can be. */
    private const val MAX_LENGTH = 42

    /** What is standing on a wire, as its own box: the whole block, since a wire is walked into. */
    public fun isStandingOn(
        x: Int,
        y: Int,
        z: Int,
        ex: Double,
        ey: Double,
        ez: Double,
    ): Boolean = ex >= x && ex <= x + 1 && ez >= z && ez <= z + 1 && ey >= y - CROSSING_SLACK && ey <= y + 1

    /**
     * parity: `checkPressed`: the wire follows whether anything is on it, books its own recheck, and
     * tells the hooks at both ends. Answers what changed.
     */
    public fun press(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        occupied: Boolean,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        if (block != WIRE) return
        val powered = BlockStateDefinitions.propertiesOf(stateId)["powered"] == "true"
        if (occupied != powered) {
            val turned =
                BlockStateDefinitions.stateIdOf(block, mapOf("powered" to occupied.toString()), stateId) ?: return
            if (world.state.setBlockState(x, y, z, turned)) changes += BlockChange(x, y, z, turned)
            for (face in HORIZONTAL) recalculateFrom(world, x, y, z, face, changes)
        }
        if (occupied) world.state.scheduleBlockTick(x, y, z, WIRE, RECHECK_TICKS)
    }

    /** Walks the line from a wire to the hook at either end and recalculates it. */
    private fun recalculateFrom(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        face: BlockFace,
        changes: MutableList<BlockChange>,
    ) {
        for (step in 1 until MAX_LENGTH) {
            val hx = x + face.stepX * step
            val hz = z + face.stepZ * step
            val stateId = world.state.getBlockState(hx, y, hz) ?: return
            val block = BlockStateDefinitions.blockOf(stateId) ?: return
            if (block == WIRE) continue
            if (block != HOOK) return
            calculateHook(world, hx, y, hz, changes)
            return
        }
    }

    /**
     * parity: `TripWireHookBlock.calculateState`: it looks along the way it faces for the hook
     * pointing back at it. Attached means it found one with unbroken string in between; powered
     * means one of those strings has something on it.
     */
    public fun calculateHook(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changes: MutableList<BlockChange>,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        if (block != HOOK) return
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val facing = properties["facing"] ?: return
        val direction = HORIZONTAL.firstOrNull { it.name.lowercase() == facing } ?: return

        var attached = true
        var powered = false
        var receiver = 0
        for (step in 1 until MAX_LENGTH) {
            val tx = x + direction.stepX * step
            val tz = z + direction.stepZ * step
            val alongId = world.state.getBlockState(tx, y, tz) ?: break
            val along = BlockStateDefinitions.blockOf(alongId)
            if (along == HOOK) {
                val other = BlockStateDefinitions.propertiesOf(alongId)["facing"]
                if (other == direction.opposite.name.lowercase()) receiver = step
                break
            }
            if (along != WIRE) {
                attached = false
                continue
            }
            val wire = BlockStateDefinitions.propertiesOf(alongId)
            // parity: a wire cut with shears is disarmed, and a disarmed wire never powers.
            if (wire["disarmed"] != "true" && wire["powered"] == "true") powered = true
        }
        // parity: `attached &= receiverPos > 1`, so two hooks side by side are not a line.
        attached = attached && receiver > 1
        powered = powered && attached

        write(world, x, y, z, stateId, block, attached, powered, changes)
        if (receiver > 0) {
            val ox = x + direction.stepX * receiver
            val oz = z + direction.stepZ * receiver
            val otherId = world.state.getBlockState(ox, y, oz) ?: return
            val otherBlock = BlockStateDefinitions.blockOf(otherId) ?: return
            write(world, ox, y, oz, otherId, otherBlock, attached, powered, changes)
        }
        // parity: the wires themselves follow the pair, which is what draws them taut.
        for (step in 1 until maxOf(receiver, 1)) {
            val tx = x + direction.stepX * step
            val tz = z + direction.stepZ * step
            val alongId = world.state.getBlockState(tx, y, tz) ?: continue
            val along = BlockStateDefinitions.blockOf(alongId) ?: continue
            if (along != WIRE) continue
            write(world, tx, y, tz, alongId, along, attached, null, changes)
        }
    }

    private fun write(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        block: Identifier,
        attached: Boolean,
        powered: Boolean?,
        changes: MutableList<BlockChange>,
    ) {
        val wanted = mutableMapOf("attached" to attached.toString())
        if (powered != null) wanted["powered"] = powered.toString()
        val turned = BlockStateDefinitions.stateIdOf(block, wanted, stateId) ?: return
        if (turned == stateId) return
        if (world.state.setBlockState(x, y, z, turned)) changes += BlockChange(x, y, z, turned)
    }

    private val HORIZONTAL = listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST, BlockFace.EAST)

    /** How far below the block a thing still counts as crossing it, since a wire lies on the floor. */
    private const val CROSSING_SLACK = 0.0625
}
