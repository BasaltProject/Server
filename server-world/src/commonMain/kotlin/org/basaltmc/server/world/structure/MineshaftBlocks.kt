package org.basaltmc.server.world.structure

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/**
 * The block states a mineshaft is framed in, which is all its type decides.
 * // parity: MineshaftStructure.Type, oak for a normal shaft and dark oak for a badlands one.
 */
public class MineshaftBlocks(
    type: MineshaftType,
) {
    private val wood = if (type == MineshaftType.MESA) "dark_oak" else "oak"

    public val log: Int = state("${wood}_log")
    public val planks: Int = state("${wood}_planks")

    /** A plain fence post, before the sides a support turns on. */
    public val fence: Int = state("${wood}_fence")

    public val fenceWest: Int = fenceWith("west")
    public val fenceEast: Int = fenceWith("east")

    private fun fenceWith(side: String): Int =
        Blocks.stateIdOf(
            Identifier.vanilla("${wood}_fence"),
            FENCE_SIDES.associateWith { if (it == side) "true" else "false" } + ("waterlogged" to "false"),
        ) ?: fence

    private companion object {
        val FENCE_SIDES = listOf("north", "south", "east", "west")

        fun state(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
    }
}
