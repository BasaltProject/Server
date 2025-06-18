package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Sand and gravel coming down.
 *
 * parity: `FallingBlock` over `FallingBlockEntity`. A block that loses what was under it schedules
 * itself two ticks out, and that tick turns it into an entity that falls at vanilla's own rate and
 * becomes a block again where it lands.
 */
@OptIn(ExperimentalUuidApi::class)
public object FallingBlocks {
    /** parity: `FallingBlock.getDelayAfterPlace`. */
    public const val FALL_DELAY_TICKS: Int = 2

    /** parity: `Entity.getGravity` for a falling block, and `Entity.getAirDrag`. */
    private const val GRAVITY = 0.04
    private const val AIR_DRAG = 0.98

    /** The entity, and what a scheduled tick on one of these blocks is filed under. */
    public val ENTITY_TYPE: Identifier = Identifier.vanilla("falling_block")

    public fun falls(stateId: Int): Boolean = BlockConnections.isFalling(stateId)

    /** parity: `FallingBlock.isFree`: air, fire, a fluid or anything that gives way. */
    public fun isFree(stateId: Int): Boolean =
        stateId == Blocks.AIR.stateId ||
            BlockShapes.isReplaceable(stateId) ||
            BlockStateDefinitions.blockOf(stateId)?.path == "fire"

    /** Asks for [x], [y], [z] to be looked at in two ticks, if what stands there can fall at all. */
    public fun scheduleIfFalling(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val stateId = world.state.getBlockState(x, y, z) ?: return
        if (!falls(stateId)) return
        world.state.scheduleBlockTick(x, y, z, ENTITY_TYPE, FALL_DELAY_TICKS)
    }

    /**
     * Turns the block into an entity if it still has nothing under it. Answers it, or null if the
     * tick came due over something solid after all.
     */
    public fun beginFalling(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        allocateEntityId: () -> Int,
    ): ServerEntity? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (!falls(stateId)) return null
        if (y <= world.minY) return null
        if (!isFree(world.state.getBlockState(x, y - 1, z) ?: return null)) return null
        val protocolType = EntityTypes[ENTITY_TYPE]?.protocolId ?: return null
        if (!world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) return null
        val entity =
            ServerEntity(
                entityId = allocateEntityId(),
                uuid = Uuid.random(),
                type = ENTITY_TYPE,
                protocolType = protocolType,
                // parity: `FallingBlockEntity.fall` spawns it at the middle of the block it left.
                x = x + 0.5,
                y = y.toDouble(),
                z = z + 0.5,
            )
        entity.objectData = stateId
        return world.state.spawnEntity(entity)
    }

    /**
     * Moves one falling block a tick on, and lands it. Answers the block it became, if it landed.
     *
     * parity: `FallingBlockEntity.tick`, which adds gravity, scales by the air drag and stops on
     * the first block that will not give way. What vanilla does with the leftover (an anvil that
     * cannot be placed drops as an item) is not here: this build only puts the block back.
     */
    public fun tick(
        world: LoadedWorld,
        entity: ServerEntity,
    ): BlockChange? {
        entity.velocityY -= GRAVITY
        entity.velocityY *= AIR_DRAG
        val nextY = entity.y + entity.velocityY
        val blockX = kotlin.math.floor(entity.x).toInt()
        val blockZ = kotlin.math.floor(entity.z).toInt()
        val landingY = kotlin.math.floor(nextY).toInt()

        val below = world.state.getBlockState(blockX, landingY, blockZ)
        if (below != null && !isFree(below)) {
            entity.removed = true
            val resting = landingY + 1
            val stateId = entity.objectData
            if (world.state.setBlockState(blockX, resting, blockZ, stateId)) {
                return BlockChange(blockX, resting, blockZ, stateId)
            }
            return null
        }
        if (landingY < world.minY) {
            entity.removed = true
            return null
        }
        entity.y = nextY
        return null
    }
}
