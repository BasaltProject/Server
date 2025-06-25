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
            "crafter" to 9,
            // parity: `BrewingStandBlock.getAnalogOutputSignal`, its five slots read like any container.
            "brewing_stand" to BrewingStands.SIZE,
            // A jukebox holds one disc, and what a comparator reads off it is the song, not the slot.
            "jukebox" to 1,
            // A lectern holds one book, and what a comparator reads off it is the page.
            "lectern" to 1,
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
        stateSignal(path, BlockStateDefinitions.propertiesOf(stateId))?.let { return it }
        val size = SIZES[path] ?: return null
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return 0
        val entity = chunk.blockEntityAt(x and 15, y, z and 15) ?: return 0
        // parity: `CrafterBlockEntity.getRedstoneSignal` counts slots, not how full they are.
        if (path == "crafter") return Crafters.comparatorSignal(entity.data)
        // parity: `JukeboxBlock.getAnalogOutputSignal`, the number of the song that is playing.
        if (path == "jukebox") return Jukeboxes.comparatorOutputAt(entity.data)
        // parity: `LecternBlock.getAnalogOutputSignal`, which is nothing at all without a book, and
        // otherwise how far through it the reader is.
        if (path == "lectern") {
            val hasBook = BlockStateDefinitions.propertiesOf(stateId)["has_book"] == "true"
            return if (hasBook) Lecterns.comparatorSignal(entity.data) else 0
        }
        val slots = BlockInventory.read(entity.data, size).map { (it?.count ?: 0) to DEFAULT_MAX_STACK }
        return Redstone.containerSignal(slots, size)
    }

    /**
     * The blocks a comparator reads straight off their own state, with no contents involved.
     *
     * parity: each block's `getAnalogOutputSignal`. They are one line each in the jar and they are
     * one line each here; what they have in common is that the number is already in the state.
     */
    private fun stateSignal(
        path: String,
        properties: Map<String, String>,
    ): Int? =
        when {
            // parity: `CakeBlock.getOutputSignal`, two per slice left, so a whole cake reads fourteen.
            path == "cake" -> (FULL_CAKE - (properties["bites"]?.toIntOrNull() ?: 0)) * 2
            // parity: `CandleCakeBlock`, always a full cake: a cake with a candle has no bites.
            path.endsWith("candle_cake") -> FULL_CAKE * 2
            path == "composter" -> properties["level"]?.toIntOrNull() ?: 0
            path == "water_cauldron" || path == "powder_snow_cauldron" -> properties["level"]?.toIntOrNull() ?: 0
            // parity: `LavaCauldronBlock`, which is always three because lava has no levels.
            path == "lava_cauldron" -> LAVA_CAULDRON_SIGNAL
            // parity: `RespawnAnchorBlock.getScaledChargeLevel`, four charges spread over fifteen.
            path == "respawn_anchor" ->
                ((properties["charges"]?.toIntOrNull() ?: 0) / MAX_CHARGES.toFloat() * Redstone.MAX_SIGNAL).toInt()
            path == "end_portal_frame" -> if (properties["eye"] == "true") Redstone.MAX_SIGNAL else 0
            path == "beehive" || path == "bee_nest" -> properties["honey_level"]?.toIntOrNull() ?: 0
            path.endsWith("copper_bulb") -> if (properties["lit"] == "true") Redstone.MAX_SIGNAL else 0
            // parity: `CopperGolemStatueBlock`, one per pose in the order the enum declares them.
            path == "copper_golem_statue" || path.endsWith("_copper_golem_statue") ->
                GOLEM_POSES.indexOf(properties["copper_golem_pose"]).let { if (it < 0) 0 else it + 1 }
            else -> null
        }

    /** parity: `CakeBlock`'s seven slices, which is what `getOutputSignal` counts down from. */
    private const val FULL_CAKE = 7
    private const val LAVA_CAULDRON_SIGNAL = 3
    private const val MAX_CHARGES = 4

    /** parity: `CopperGolemStatueBlock.Pose`, in its declared order, which is what the signal counts. */
    private val GOLEM_POSES = listOf("standing", "sitting", "running", "star")
}
