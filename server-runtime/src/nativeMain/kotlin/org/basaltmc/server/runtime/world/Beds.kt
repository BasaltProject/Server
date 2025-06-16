package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.wire.common.Identifier

/** Why a bed refused. parity: `Player.BedSleepingProblem`, by the message it puts on the screen. */
public enum class BedProblem(
    public val translationKey: String,
) {
    NOT_POSSIBLE_NOW("block.minecraft.bed.no_sleep"),
    TOO_FAR_AWAY("block.minecraft.bed.too_far_away"),
    OCCUPIED("block.minecraft.bed.occupied"),
    NOT_HERE("block.minecraft.bed.not_valid"),
}

/**
 * Sleeping in a bed: which block is the pillow, whether it can be slept in, and what it does.
 *
 * parity: `BedBlock.useWithoutItem` and `ServerPlayer.startSleepInBed`, minus the two things this
 * build has no system for: no monster nearby stops anyone sleeping (there are no monsters), and a
 * bed in the Nether or the End is destroyed without the explosion that vanilla sets off, which is
 * the visible part of what it does but not all of it.
 */
public object Beds {
    /** parity: `Level.isNight`, which is when a bed will take anyone. */
    public const val NIGHT_START: Long = 12542
    public const val NIGHT_END: Long = 23459

    /** parity: `ServerPlayer.bedInRange`, three blocks from the head of the bed. */
    public const val RANGE: Double = 3.0

    /** parity: `ServerLevel.tickTime`'s wake-up: the sleepers have to hold it for five seconds. */
    public const val SLEEP_TICKS_BEFORE_MORNING: Int = 100

    public fun isBed(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path?.endsWith("_bed") == true

    public fun isOccupied(stateId: Int): Boolean = BlockStateDefinitions.propertiesOf(stateId)["occupied"] == "true"

    public fun facingOf(stateId: Int): String? = BlockStateDefinitions.propertiesOf(stateId)["facing"]

    public fun isHead(stateId: Int): Boolean = BlockStateDefinitions.propertiesOf(stateId)["part"] == "head"

    /** The same bed with its occupied flag set, or the state unchanged if it is not a bed. */
    public fun withOccupied(
        stateId: Int,
        occupied: Boolean,
    ): Int {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return stateId
        return BlockStateDefinitions.stateIdOf(
            block,
            mapOf("occupied" to occupied.toString()),
            fromStateId = stateId,
        ) ?: stateId
    }

    /** The step from the foot of a bed to its head, which is where the pillow is. */
    public fun headOffset(stateId: Int): IntArray =
        when (facingOf(stateId)) {
            "north" -> intArrayOf(0, 0, -1)
            "south" -> intArrayOf(0, 0, 1)
            "west" -> intArrayOf(-1, 0, 0)
            "east" -> intArrayOf(1, 0, 0)
            else -> intArrayOf(0, 0, 0)
        }

    /** parity: a bed only works in a dimension whose rule says so, and blows up in the others. */
    public fun explodesIn(dimension: WorldDimension): Boolean = dimension != WorldDimension.OVERWORLD

    public fun isNight(timeOfDay: Long): Boolean = timeOfDay % DAY_TICKS in NIGHT_START..NIGHT_END

    /** parity: `ServerLevel.wakeUpAllPlayers`, which puts the clock at the next morning. */
    public fun nextMorning(timeOfDay: Long): Long = (timeOfDay / DAY_TICKS + 1) * DAY_TICKS

    /** Takes both halves of the bed away, which is what an exploding bed leaves behind. */
    public fun destroy(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray> {
        val state = world.state.getBlockState(x, y, z) ?: return emptyList()
        if (!isBed(state)) return emptyList()
        val removed = mutableListOf<IntArray>()
        val offset = headOffset(state)
        val other =
            if (isHead(state)) {
                intArrayOf(x - offset[0], y, z - offset[2])
            } else {
                intArrayOf(x + offset[0], y, z + offset[2])
            }
        for (block in listOf(intArrayOf(x, y, z), other)) {
            val at = world.state.getBlockState(block[0], block[1], block[2]) ?: continue
            if (!isBed(at)) continue
            world.state.setBlockState(block[0], block[1], block[2], Blocks.AIR.stateId)
            removed += intArrayOf(block[0], block[1], block[2], Blocks.AIR.stateId)
        }
        return removed
    }

    private const val DAY_TICKS = 24000L

    /** Every bed item, for the placement rules that need to know one when they see one. */
    public fun isBedItem(item: Identifier): Boolean = item.path.endsWith("_bed")
}
