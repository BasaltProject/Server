package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions

/**
 * What a container holds, seen from outside: how full it is, which is all a comparator wants.
 *
 * parity: `BlockState.getAnalogOutputSignal` over `AbstractContainerMenu.getRedstoneSignalFromContainer`.
 * Twenty-seven blocks implement it in 26.2; the ones whose contents this build keeps are here, and
 * the rest answer null rather than a wrong zero, so a comparator behind them reads the block instead
 * of being told the box is empty.
 */
public object Containers {
    /**
     * How many slots each container has, which is the denominator of the fullness.
     *
     * A double chest reads as one container of 54 in vanilla; pairing is not modelled yet, so each
     * half answers for its own 27. Marked here rather than answering a confident wrong number.
     */
    public val SIZES: Map<String, Int> =
        mapOf(
            "furnace" to 3,
            "blast_furnace" to 3,
            "smoker" to 3,
            "chest" to 27,
            "trapped_chest" to 27,
            "barrel" to 27,
            "hopper" to 5,
            "dispenser" to 9,
            "dropper" to 9,
        )

    /** The blocks that only hold, with no machinery of their own. */
    public val PLAIN: Set<String> = setOf("chest", "trapped_chest", "barrel", "hopper", "dispenser", "dropper")

    /** parity: `Item.getMaxStackSize`, which this build does not read yet: everything stacks to 64. */
    private const val DEFAULT_MAX_STACK = 64

    /**
     * The comparator reading of the container at these coordinates, or null when the block is not
     * one this build keeps contents for.
     */
    public fun analogSignal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        val size = SIZES[path] ?: return null
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return 0
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return 0
        val slots = BlockInventory.read(entity.data, size).map { (it?.count ?: 0) to DEFAULT_MAX_STACK }
        return Redstone.containerSignal(slots, size)
    }
}
