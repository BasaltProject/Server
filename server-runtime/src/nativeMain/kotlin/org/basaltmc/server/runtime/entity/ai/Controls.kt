package org.basaltmc.server.runtime.entity.ai

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.runtime.entity.LivingMovement
import org.basaltmc.server.runtime.entity.PathFinder
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.floor
import kotlin.math.sqrt

/**
 * The path a mob is walking and how far along it is.
 *
 * parity: `PathNavigation`: the path is a list of nodes, the mob steps to the next one when it is
 * close enough to the current one, and what it hands the move control is that node's middle.
 */
public object Navigation {
    /** parity: `PathNavigation.maxDistanceToWaypoint`, which is the mob's own width or 0.75. */
    private fun waypointRange(mob: ServerEntity): Double {
        val width = mob.width
        return if (width > 0.75) width / 2.0 else 0.75 - width / 2.0
    }

    /** parity: `PathNavigation.moveTo`: works out a path and starts walking it. */
    public fun moveTo(
        world: WorldState,
        mob: ServerEntity,
        x: Double,
        y: Double,
        z: Double,
        speedModifier: Double,
    ): Boolean {
        val path =
            PathFinder.find(
                world,
                floor(mob.x).toInt(),
                floor(mob.y).toInt(),
                floor(mob.z).toInt(),
                floor(x).toInt(),
                floor(y).toInt(),
                floor(z).toInt(),
            ) ?: return false
        if (path.isEmpty()) return false
        mob.path = path
        mob.pathIndex = 0
        mob.walkTarget = doubleArrayOf(x, y, z)
        mob.moveSpeed = speedModifier
        return true
    }

    /** Whether a path to there exists at all. parity: `PathNavigation.createPath` before a goal starts. */
    public fun canReach(
        world: WorldState,
        mob: ServerEntity,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val path =
            PathFinder.find(
                world,
                floor(mob.x).toInt(),
                floor(mob.y).toInt(),
                floor(mob.z).toInt(),
                floor(x).toInt(),
                floor(y).toInt(),
                floor(z).toInt(),
            )
        return path != null && path.isNotEmpty()
    }

    public fun isDone(mob: ServerEntity): Boolean {
        val path = mob.path ?: return true
        return mob.pathIndex >= path.size
    }

    public fun stop(mob: ServerEntity) {
        mob.path = null
        mob.walkTarget = null
        mob.pathIndex = 0
    }

    /** parity: `PathNavigation.tick` over `followThePath`. */
    public fun tick(
        world: WorldState,
        mob: ServerEntity,
    ) {
        val path = mob.path ?: return
        if (isDone(mob)) {
            stop(mob)
            return
        }
        val range = waypointRange(mob)
        while (mob.pathIndex < path.size) {
            val node = path[mob.pathIndex]
            val dx = abs(mob.x - (node[0] + 0.5))
            val dy = abs(mob.y - node[1])
            val dz = abs(mob.z - (node[2] + 0.5))
            if (dx >= range || dz >= range || dy >= 1.0) break
            mob.pathIndex++
        }
        if (isDone(mob)) {
            stop(mob)
            return
        }
        val node = path[mob.pathIndex]
        MoveControl.setWanted(mob, node[0] + 0.5, node[1].toDouble(), node[2] + 0.5, mob.moveSpeed)
    }
}

/**
 * The turn and the step a mob takes towards where it wants to be.
 *
 * parity: `MoveControl.tick`'s `MOVE_TO` branch: turn at most ninety degrees a tick, walk at the
 * goal's own share of the movement speed, and jump at whatever the step cannot climb.
 */
public object MoveControl {
    /** parity: the 2.5e-7 under which vanilla calls it arrived. */
    private const val ARRIVED = 2.5000003e-7

    /** parity: `rotlerp`'s own ninety degrees a tick. */
    private const val TURN_RATE = 90f

    public fun setWanted(
        mob: ServerEntity,
        x: Double,
        y: Double,
        z: Double,
        speedModifier: Double,
    ) {
        mob.wantedX = x
        mob.wantedY = y
        mob.wantedZ = z
        mob.moveSpeed = speedModifier
        mob.moving = true
    }

    /** parity: `CubeMobMoveControl`, which only ever sets a speed: the way it faces is the way it goes. */
    public fun setForward(
        mob: ServerEntity,
        speedModifier: Double,
    ) {
        mob.moveSpeed = speedModifier
        mob.movingForward = true
    }

    public fun stop(mob: ServerEntity) {
        mob.moving = false
        mob.movingForward = false
    }

    public fun tick(
        world: WorldState,
        mob: ServerEntity,
        speed: Double,
    ) {
        // parity: `LivingEntity.travel` picks the fluid path first: a squid pushes itself along in
        // water rather than walking, and nothing in water falls like a stone.
        if (inWater(world, mob)) {
            LivingMovement.travelInWater(world, mob, forward = 0.0, strafe = 0.0)
            mob.moving = false
            return
        }
        if (mob.movingForward) {
            mob.movingForward = false
            val ahead = speed * mob.moveSpeed
            LivingMovement.travel(world, mob, forward = ahead, strafe = 0.0, speed = ahead)
            return
        }
        if (!mob.moving) {
            LivingMovement.travel(world, mob, forward = 0.0, strafe = 0.0, speed = 0.0)
            return
        }
        mob.moving = false
        val dx = mob.wantedX - mob.x
        val dy = mob.wantedY - mob.y
        val dz = mob.wantedZ - mob.z
        if (dx * dx + dy * dy + dz * dz < ARRIVED) {
            LivingMovement.travel(world, mob, forward = 0.0, strafe = 0.0, speed = 0.0)
            return
        }
        val wanted = (atan2(dz, dx) * DEGREES).toFloat() - 90f
        mob.yaw = rotlerp(mob.yaw, wanted, TURN_RATE)
        // parity: `Mob.setSpeed`, which also sets the forward input to the same number, so what a
        // mob accelerates by is the speed times itself. Passing one here made every mob run.
        val walk = speed * mob.moveSpeed
        // parity: the jump when the next step is above what the mob can climb, or when it is stood
        // inside something with a collision box.
        if (dy > LivingMovement.STEP_HEIGHT && dx * dx + dz * dz < maxOf(1.0, mob.width)) {
            LivingMovement.jump(world, mob)
        } else if (mob.horizontallyBlocked && mob.onGround) {
            LivingMovement.jump(world, mob)
        }
        LivingMovement.travel(world, mob, forward = walk, strafe = 0.0, speed = walk)
    }

    /** parity: `MoveControl.rotlerp`, the shortest way round capped at [max] degrees. */
    public fun rotlerp(
        from: Float,
        to: Float,
        max: Float,
    ): Float {
        var difference = wrapDegrees(to - from)
        if (difference > max) difference = max
        if (difference < -max) difference = -max
        return from + difference
    }

    public fun wrapDegrees(degrees: Float): Float {
        var wrapped = degrees % 360f
        if (wrapped >= 180f) wrapped -= 360f
        if (wrapped < -180f) wrapped += 360f
        return wrapped
    }

    private const val DEGREES = 180.0 / PI
}

/**
 * Where a mob is looking.
 *
 * parity: `LookControl`: the head turns towards whatever a goal pointed it at, at most so many
 * degrees a tick, and drifts back to the body when nothing is asking.
 */
public object LookControl {
    /** parity: `LookControl.tick`, which brings the head back to the body ten degrees at a time. */
    private const val DRIFT_RATE = 10f

    public fun lookAt(
        mob: ServerEntity,
        x: Double,
        y: Double,
        z: Double,
        maxYaw: Float = 10f,
        maxPitch: Float = 40f,
    ) {
        mob.lookX = x
        mob.lookY = y
        mob.lookZ = z
        mob.lookTicks = 2
        mob.lookYawSpeed = maxYaw
        mob.lookPitchSpeed = maxPitch
    }

    public fun tick(mob: ServerEntity) {
        if (mob.lookTicks > 0) {
            mob.lookTicks--
            val dx = mob.lookX - mob.x
            val dy = mob.lookY - (mob.y + mob.height * EYE_SHARE)
            val dz = mob.lookZ - mob.z
            val horizontal = sqrt(dx * dx + dz * dz)
            val wantedYaw = (atan2(dz, dx) * DEGREES).toFloat() - 90f
            val wantedPitch = -(atan2(dy, horizontal) * DEGREES).toFloat()
            mob.headYaw = MoveControl.rotlerp(mob.headYaw, wantedYaw, mob.lookYawSpeed)
            mob.pitch = MoveControl.rotlerp(mob.pitch, wantedPitch, mob.lookPitchSpeed)
        } else {
            mob.headYaw = MoveControl.rotlerp(mob.headYaw, mob.yaw, DRIFT_RATE)
        }
        // parity: `clampHeadRotationToBody`, which is what stops a walking mob looking backwards.
        if (mob.path != null) mob.headYaw = MoveControl.rotlerp(mob.headYaw, mob.yaw, MAX_HEAD_TURN)
    }

    private const val EYE_SHARE = 0.85
    private const val MAX_HEAD_TURN = 75f
    private const val DEGREES = 180.0 / PI
}

/** parity: `Entity.isInWater`, which is the block its feet are in. */
internal fun inWater(
    world: WorldState,
    mob: ServerEntity,
): Boolean {
    val state = world.getBlockState(floor(mob.x).toInt(), floor(mob.y).toInt(), floor(mob.z).toInt()) ?: return false
    return org.basaltmc.server.registry.BlockStateDefinitions
        .blockOf(state)
        ?.path == "water"
}

/** Whether the block at these coordinates is one a mob can stand in. */
internal fun isPassable(
    world: WorldState,
    x: Int,
    y: Int,
    z: Int,
): Boolean = BlockShapes.isEmptyShape(world.getBlockState(x, y, z) ?: return false)
