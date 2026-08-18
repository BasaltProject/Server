package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions

/**
 * The two halves of a double chest, and which of them the window shows first.
 *
 * parity: `ChestBlock.getConnectedDirection` and `DOUBLE_INVENTORY_COMBINER`. Which half comes first
 * is not "the one that was clicked": a right half leads, a left half follows, so both chests show
 * the same six rows in the same order whichever one you open.
 */
public object DoubleChests {
    /**
     * The pair at these coordinates, first half first, or null when this is a single chest.
     */
    public fun halves(
        world: LoadedWorld?,
        x: Int,
        y: Int,
        z: Int,
    ): Pair<IntArray, IntArray>? {
        val loaded = world ?: return null
        val stateId = loaded.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block.path !in Containers.CHESTS) return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val type = properties["type"] ?: return null
        if (type == "single") return null
        val facing = properties["facing"] ?: return null
        // parity: `getConnectedDirection`: a left half looks clockwise for its partner, a right one
        // counter-clockwise.
        val towards = if (type == "left") clockwise(facing) else counterClockwise(facing)
        val partner = intArrayOf(x + stepX(towards), y, z + stepZ(towards))
        val partnerState = loaded.state.getBlockState(partner[0], partner[1], partner[2]) ?: return null
        if (BlockStateDefinitions.blockOf(partnerState) != block) return null
        val theirs = BlockStateDefinitions.propertiesOf(partnerState)
        if (theirs["facing"] != facing) return null
        if (theirs["type"] != (if (type == "left") "right" else "left")) return null
        val self = intArrayOf(x, y, z)
        return if (type == "right") self to partner else partner to self
    }

    private fun clockwise(facing: String): String =
        when (facing) {
            "north" -> "east"
            "east" -> "south"
            "south" -> "west"
            else -> "north"
        }

    private fun counterClockwise(facing: String): String = clockwise(clockwise(clockwise(facing)))

    private fun stepX(facing: String): Int =
        when (facing) {
            "west" -> -1
            "east" -> 1
            else -> 0
        }

    private fun stepZ(facing: String): Int =
        when (facing) {
            "north" -> -1
            "south" -> 1
            else -> 0
        }
}
