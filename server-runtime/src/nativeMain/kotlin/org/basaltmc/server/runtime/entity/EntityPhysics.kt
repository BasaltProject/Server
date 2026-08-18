package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockMovement
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.floor

/**
 * One tick of movement for an entity that falls.
 *
 * parity: `Entity.applyGravity`, `Entity.move` and the drag each class applies afterwards. The
 * numbers differ per entity (an item falls at 0.04 a tick, an arrow at 0.05), so they are handed in
 * rather than guessed here; what is shared is the order: gravity, then the move against the blocks,
 * then the drag on what is left.
 */
public object EntityPhysics {
    /** parity: `Entity.getAirDrag`, the 0.98 nearly everything keeps. */
    public const val AIR_DRAG: Double = 0.98

    /** parity: `Entity.getBlockBounciness`, which gives anything that is not alive four fifths. */
    private const val NON_LIVING_BOUNCE_SHARE = 0.8f

    /** parity: `getBlockPosBelowThatAffectsMyMovement`, which looks half a block under the feet. */
    private const val FRICTION_PROBE_DEPTH = 0.5

    private val movement = DoubleArray(3)

    /**
     * Moves [entity] by its own velocity, with [gravity] taken off first.
     *
     * [horizontalGroundDrag] is what a landing costs sideways (a TNT block keeps 0.7 of it), and
     * [verticalGroundDrag] what a landing does to the fall (vanilla's own -0.5 bounce).
     */
    public fun move(
        world: WorldState,
        entity: ServerEntity,
        gravity: Double,
        drag: Double = AIR_DRAG,
        horizontalGroundDrag: Double = 1.0,
        verticalGroundDrag: Double = 1.0,
    ) {
        entity.velocityY -= gravity

        val box = Aabb.of(entity.x, entity.y, entity.z, entity.width, entity.height)
        EntityCollision.collide(world, box, entity.velocityX, entity.velocityY, entity.velocityZ, movement)
        val movedX = movement[0]
        val movedY = movement[1]
        val movedZ = movement[2]
        entity.x += movedX
        entity.y += movedY
        entity.z += movedZ
        entity.onGround = movedY != entity.velocityY && entity.velocityY < 0.0

        // parity: `Entity.restituteMovementAfterCollisions`: a stopped axis is zeroed unless the
        // block underneath bounces, and only slime and beds do.
        val bounciness =
            if (entity.onGround) {
                BlockMovement.bouncinessOf(blockUnder(world, entity)) * NON_LIVING_BOUNCE_SHARE
            } else {
                0f
            }
        if (movedX != entity.velocityX) entity.velocityX = 0.0
        if (movedZ != entity.velocityZ) entity.velocityZ = 0.0
        if (movedY != entity.velocityY) {
            entity.velocityY = if (bounciness > 0f) -entity.velocityY * bounciness else 0.0
        }

        entity.velocityX *= drag
        entity.velocityY *= drag
        entity.velocityZ *= drag
        if (entity.onGround) {
            entity.velocityX *= horizontalGroundDrag
            entity.velocityY *= verticalGroundDrag
            entity.velocityZ *= horizontalGroundDrag
        }
    }

    /** parity: `getBlockPosBelowThatAffectsMyMovement`, the block whose friction it feels. */
    private fun blockUnder(
        world: WorldState,
        entity: ServerEntity,
    ): Int =
        world.getBlockState(
            floor(entity.x).toInt(),
            floor(entity.y - FRICTION_PROBE_DEPTH).toInt(),
            floor(entity.z).toInt(),
        ) ?: 0
}
