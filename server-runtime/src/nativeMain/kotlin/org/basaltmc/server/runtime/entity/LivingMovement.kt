package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockMovement
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * How something alive walks.
 *
 * parity: `LivingEntity.travelInAir`, which is where the whole feel of walking comes from: the speed
 * an entity asks for is divided by the cube of the friction under it, and then that same friction is
 * taken back off the movement afterwards. On ice the two do not cancel, which is why it slides.
 */
public object LivingMovement {
    /** parity: `LivingEntity.DEFAULT_BASE_GRAVITY`. */
    public const val GRAVITY: Double = 0.08

    /** parity: `BASE_JUMP_POWER`, what one jump is worth before any block or effect changes it. */
    public const val JUMP_POWER: Double = 0.42

    /** parity: the 0.91 of air drag and the 0.98 the vertical keeps. */
    private const val AIR_DRAG = 0.91
    private const val VERTICAL_DRAG = 0.98

    /** parity: `getFrictionInfluencedSpeed`'s own constant. */
    private const val FRICTION_SPEED = 0.21600002

    /** parity: `Attributes.STEP_HEIGHT`'s default, which is what a mob climbs without jumping. */
    public const val STEP_HEIGHT: Double = 0.6

    /** parity: `getFlyingSpeed`, what steering in mid-air is worth. */
    private const val FLYING_SPEED = 0.02

    private val movement = DoubleArray(3)

    /**
     * One tick of walking. [forward] and [strafe] are the input, in the entity's own facing, and
     * [speed] is `Attributes.MOVEMENT_SPEED`.
     */
    public fun travel(
        world: WorldState,
        entity: ServerEntity,
        forward: Double,
        strafe: Double,
        speed: Double,
        gravity: Double = GRAVITY,
    ) {
        val blockFriction =
            if (entity.onGround) BlockMovement.frictionOf(blockUnder(world, entity)).toDouble() else 1.0
        val inputSpeed = frictionInfluencedSpeed(entity, speed, blockFriction)
        addInput(entity, forward, strafe, inputSpeed)

        val box = Aabb.of(entity.x, entity.y, entity.z, entity.width, entity.height)
        EntityCollision.collideWithStep(
            world,
            box,
            entity.velocityX,
            entity.velocityY,
            entity.velocityZ,
            STEP_HEIGHT,
            entity.onGround,
            movement,
        )
        val movedX = movement[0]
        val movedY = movement[1]
        val movedZ = movement[2]
        entity.x += movedX
        entity.y += movedY
        entity.z += movedZ
        entity.onGround = movedY != entity.velocityY && entity.velocityY < 0.0
        val blockedSideways = movedX != entity.velocityX || movedZ != entity.velocityZ
        if (movedX != entity.velocityX) entity.velocityX = 0.0
        if (movedZ != entity.velocityZ) entity.velocityZ = 0.0
        if (movedY != entity.velocityY) entity.velocityY = 0.0

        // parity: gravity comes off after the move, and the drag after that.
        entity.velocityY -= gravity
        val horizontalDrag = blockFriction * AIR_DRAG
        entity.velocityX *= horizontalDrag
        entity.velocityY *= VERTICAL_DRAG
        entity.velocityZ *= horizontalDrag
        entity.horizontallyBlocked = blockedSideways
    }

    /** parity: `LivingEntity.jumpFromGround`, plus the block's own jump factor. */
    public fun jump(
        world: WorldState,
        entity: ServerEntity,
    ) {
        if (!entity.onGround) return
        entity.velocityY = JUMP_POWER * BlockMovement.jumpFactorOf(blockUnder(world, entity))
        entity.onGround = false
    }

    /** parity: `getFrictionInfluencedSpeed`: slippery ground asks for more, so the drag cancels less. */
    private fun frictionInfluencedSpeed(
        entity: ServerEntity,
        speed: Double,
        blockFriction: Double,
    ): Double =
        when {
            !entity.onGround -> FLYING_SPEED
            blockFriction > BlockMovement.DEFAULT_FRICTION ->
                speed * (FRICTION_SPEED / (blockFriction * blockFriction * blockFriction))
            else -> speed
        }

    /** parity: `Entity.getInputVector`, which turns the input into world axes through the yaw. */
    private fun addInput(
        entity: ServerEntity,
        forward: Double,
        strafe: Double,
        speed: Double,
    ) {
        val length = forward * forward + strafe * strafe
        if (length < 1.0e-7) return
        val scale = if (length > 1.0) speed / sqrt(length) else speed
        val x = strafe * scale
        val z = forward * scale
        val radians = entity.yaw * PI / 180.0
        val sinYaw = sin(radians)
        val cosYaw = cos(radians)
        entity.velocityX += x * cosYaw - z * sinYaw
        entity.velocityZ += z * cosYaw + x * sinYaw
    }

    private fun blockUnder(
        world: WorldState,
        entity: ServerEntity,
    ): Int =
        world.getBlockState(
            floor(entity.x).toInt(),
            floor(entity.y - FRICTION_PROBE_DEPTH).toInt(),
            floor(entity.z).toInt(),
        ) ?: 0

    private const val FRICTION_PROBE_DEPTH = 0.5
}
