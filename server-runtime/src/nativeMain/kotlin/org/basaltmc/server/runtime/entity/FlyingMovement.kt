package org.basaltmc.server.runtime.entity

import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.sqrt

/**
 * How something that flies moves.
 *
 * parity: `FlyingMoveControl` with `Mob.travel`'s flying branch: nothing falls, the mob is nudged
 * towards where it wants to be a tenth of the distance at a time, and the air takes 0.91 of what is
 * left every tick. A ghast drifts because of that, rather than because anything steers it.
 */
public object FlyingMovement {
    /** parity: the 0.91 the air keeps every tick, in `travelFlying` and in every flier's move control. */
    private const val DRAG = 0.91

    private val movement = DoubleArray(3)

    /**
     * Leans towards a point. Answers false once it is there.
     *
     * parity: every flier's move control adds to the speed rather than setting a position, and each
     * has its own number for how much: the ghast's 0.1 impulse, the vex's 0.05, and the 0.02 of the
     * air input everything else flies on. That number is [step], and with the 0.91 drag it is what
     * decides how fast the mob ends up going: about eleven times the step, and no faster.
     */
    public fun approach(
        entity: ServerEntity,
        x: Double,
        y: Double,
        z: Double,
        step: Double,
    ): Boolean {
        val dx = x - entity.x
        val dy = y - entity.y
        val dz = z - entity.z
        val distance = sqrt(dx * dx + dy * dy + dz * dz)
        if (distance < ARRIVED) return false
        entity.velocityX += dx / distance * step
        entity.velocityY += dy / distance * step
        entity.velocityZ += dz / distance * step
        return true
    }

    /** The one move a flying mob makes each tick, whatever its goals decided. */
    public fun tick(
        world: WorldState,
        entity: ServerEntity,
    ) {
        move(world, entity)
    }

    private fun move(
        world: WorldState,
        entity: ServerEntity,
    ) {
        val box = Aabb.of(entity.x, entity.y, entity.z, entity.width, entity.height)
        EntityCollision.collide(world, box, entity.velocityX, entity.velocityY, entity.velocityZ, movement)
        entity.x += movement[0]
        entity.y += movement[1]
        entity.z += movement[2]
        entity.horizontallyBlocked = movement[0] != entity.velocityX || movement[2] != entity.velocityZ
        if (movement[0] != entity.velocityX) entity.velocityX = 0.0
        if (movement[1] != entity.velocityY) entity.velocityY = 0.0
        if (movement[2] != entity.velocityZ) entity.velocityZ = 0.0
        entity.velocityX *= DRAG
        entity.velocityY *= DRAG
        entity.velocityZ *= DRAG
        entity.onGround = false
    }

    private const val ARRIVED = 0.3
}

/** The mobs that fly rather than walk, which is what decides how they are moved every tick. */
public object Flyers {
    private val FLYING =
        setOf(
            "ghast",
            "happy_ghast",
            "ghastling",
            "blaze",
            "phantom",
            "vex",
            "bat",
            "allay",
            "wither",
            "ender_dragon",
            "breeze",
        )

    public fun flies(type: org.basaltmc.wire.common.Identifier): Boolean = type.path in FLYING
}
