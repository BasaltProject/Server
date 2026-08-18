package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** One block's friction, what it does to speed and jump, and how much it bounces what lands on it. */
public class BlockMovementEntry(
    public val block: String,
    public val friction: Float,
    public val speedFactor: Float,
    public val jumpFactor: Float,
    public val bounciness: Float,
)

/**
 * How a block moves whatever is standing on it.
 *
 * parity: `Block.getFriction`, `getSpeedFactor` and `getJumpFactor`. Ice is slippery, soul sand and
 * honey are slow, and honey also kills a jump. Everything else is the default, which is why only
 * the blocks that differ are in the table.
 */
public object BlockMovement {
    /** parity: `BlockBehaviour.Properties`' own defaults. */
    public const val DEFAULT_FRICTION: Float = 0.6f
    public const val DEFAULT_SPEED_FACTOR: Float = 1.0f
    public const val DEFAULT_JUMP_FACTOR: Float = 1.0f
    public const val DEFAULT_BOUNCINESS: Float = 0.0f

    private val byBlock: Map<Identifier, BlockMovementEntry> =
        ALL_BLOCK_MOVEMENT.associateBy { Identifier.vanilla(it.block) }

    public fun frictionOf(stateId: Int): Float = entryOf(stateId)?.friction ?: DEFAULT_FRICTION

    public fun speedFactorOf(stateId: Int): Float = entryOf(stateId)?.speedFactor ?: DEFAULT_SPEED_FACTOR

    public fun jumpFactorOf(stateId: Int): Float = entryOf(stateId)?.jumpFactor ?: DEFAULT_JUMP_FACTOR

    /**
     * parity: `Block.getBounceRestitution`, which is what makes slime and a bed throw back what
     * lands on them. Anything that is not a living entity keeps four fifths of it (`getBlockBounciness`).
     */
    public fun bouncinessOf(stateId: Int): Float = entryOf(stateId)?.bounciness ?: DEFAULT_BOUNCINESS

    private fun entryOf(stateId: Int): BlockMovementEntry? = BlockStateDefinitions.blockOf(stateId)?.let { byBlock[it] }
}
