package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.floor
import kotlin.math.sqrt

/**
 * What a mob does when nothing else is happening: wander, and look where it is going.
 *
 * parity: `RandomStrollGoal` over `DefaultRandomPos.getPos`: one roll in 120 a tick, ten candidate
 * positions within ten blocks across and seven up, and the best one is walked to. Walking is
 * `MoveControl`: turn towards the target, walk forward, jump at what is in the way.
 */
public object MobBrain {
    /** parity: `RandomStrollGoal`'s own 120 ticks between attempts. */
    public const val STROLL_INTERVAL: Int = 120

    /** parity: `WaterAvoidingRandomStrollGoal`'s ten across and seven up. */
    private const val STROLL_RANGE = 10
    private const val STROLL_HEIGHT = 7

    /** parity: `DefaultRandomPos.generateRandomPos`, which tries ten and keeps the best. */
    private const val CANDIDATES = 10

    /** How close counts as arrived. parity: `PathNavigation`'s own tolerance, one block. */
    private const val ARRIVED = 1.0

    /** parity: `MoveControl.tick`'s turn rate, thirty degrees a tick. */
    private const val TURN_RATE = 30f

    /**
     * One tick of wandering. [speed] is `Attributes.MOVEMENT_SPEED`, and [modifier] the goal's own
     * multiplier on it.
     */
    public fun tick(
        world: WorldState,
        mob: ServerEntity,
        random: BasaltRandom,
        speed: Double,
        modifier: Double = 1.0,
    ) {
        mob.age++
        if (mob.walkTarget == null && random.nextInt(STROLL_INTERVAL) == 0) startStroll(world, mob, random)
        walk(world, mob, speed, modifier)
    }

    /**
     * Walks whatever path the mob has, a node at a time. Answers false once there is nowhere left
     * to go.
     *
     * parity: `PathNavigation.tick`, which follows the path rather than pushing straight at the
     * target: that is what takes a mob round a wall instead of into it.
     */
    public fun walk(
        world: WorldState,
        mob: ServerEntity,
        speed: Double,
        modifier: Double = 1.0,
    ): Boolean {
        val node = nextNode(mob)
        if (node == null) {
            mob.walkTarget = null
            mob.path = null
            LivingMovement.travel(world, mob, forward = 0.0, strafe = 0.0, speed = speed)
            return false
        }
        turnTowards(mob, node[0] - mob.x, node[2] - mob.z)
        LivingMovement.travel(world, mob, forward = 1.0, strafe = 0.0, speed = speed * modifier)
        // parity: `MoveControl`'s own jump when the way is blocked but the step was not enough.
        if (mob.horizontallyBlocked && mob.onGround) LivingMovement.jump(world, mob)
        return true
    }

    /** Works out the way to a point and starts walking it. parity: `PathNavigation.moveTo`. */
    public fun pathTo(
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
            ) ?: return false
        if (path.isEmpty()) return false
        mob.walkTarget = doubleArrayOf(x, y, z)
        mob.path = path
        mob.pathIndex = 0
        return true
    }

    /** Picks somewhere to go and works out the way there; without a way, it stays put. */
    private fun startStroll(
        world: WorldState,
        mob: ServerEntity,
        random: BasaltRandom,
    ) {
        val target = strollTarget(world, mob, random) ?: return
        pathTo(world, mob, target[0], target[1], target[2])
    }

    /** The node it is walking to now, stepping on when it arrives. Null when the path is walked out. */
    private fun nextNode(mob: ServerEntity): DoubleArray? {
        val path = mob.path ?: return mob.walkTarget?.takeIf { farFrom(mob, it) }
        while (mob.pathIndex < path.size) {
            val node = path[mob.pathIndex]
            val centre = doubleArrayOf(node[0] + 0.5, node[1].toDouble(), node[2] + 0.5)
            if (farFrom(mob, centre)) return centre
            mob.pathIndex++
        }
        return null
    }

    private fun farFrom(
        mob: ServerEntity,
        point: DoubleArray,
    ): Boolean {
        val dx = point[0] - mob.x
        val dz = point[2] - mob.z
        return dx * dx + dz * dz >= ARRIVED * ARRIVED
    }

    /**
     * parity: `DefaultRandomPos.getPos`: ten tries at a spot within range, and the one standing on
     * the most solid ground wins. A spot with nothing under it is never chosen.
     */
    public fun strollTarget(
        world: WorldState,
        mob: ServerEntity,
        random: BasaltRandom,
        range: Int = STROLL_RANGE,
        height: Int = STROLL_HEIGHT,
    ): DoubleArray? {
        var best: DoubleArray? = null
        var bestWeight = Double.NEGATIVE_INFINITY
        repeat(CANDIDATES) {
            val x = floor(mob.x).toInt() + random.nextInt(2 * range + 1) - range
            val y = floor(mob.y).toInt() + random.nextInt(2 * height + 1) - height
            val z = floor(mob.z).toInt() + random.nextInt(2 * range + 1) - range
            // parity: `LandRandomPos.movePosUpOutOfSolid`, which lifts a spot out of the ground
            // rather than throwing it away: on real terrain nearly every raw guess is inside a hill.
            val standing = groundAt(world, x, y, z) ?: return@repeat
            val weight = walkTargetValue(world, standing[0], standing[1], standing[2])
            if (weight <= bestWeight) return@repeat
            bestWeight = weight
            best = doubleArrayOf(standing[0] + 0.5, standing[1].toDouble(), standing[2] + 0.5)
        }
        return best
    }

    /**
     * The spot a mob would stand on at these coordinates: out of the ground if it is inside it, and
     * down onto it if it is over it.
     *
     * parity: `RandomPos.moveUpOutOfSolid` and the drop `WalkNodeEvaluator` allows underneath.
     */
    private fun groundAt(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): IntArray? {
        var top = y
        while (top <= y + LIFT_LIMIT && !isPassable(world, x, top, z)) top++
        if (top > y + LIFT_LIMIT) return null
        var standing = top
        var dropped = 0
        while (dropped < DROP_LIMIT && isPassable(world, x, standing - 1, z)) {
            standing--
            dropped++
        }
        val below = world.getBlockState(x, standing - 1, z) ?: return null
        if (BlockShapes.isEmptyShape(below)) return null
        return intArrayOf(x, standing, z)
    }

    private fun isPassable(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockShapes.isEmptyShape(world.getBlockState(x, y, z) ?: return false)

    /** parity: how far vanilla lifts a spot out of a hill, and how far a mob will step down to one. */
    private const val LIFT_LIMIT = 6
    private const val DROP_LIMIT = 4

    /**
     * parity: `PathfinderMob.getWalkTargetValue`, which is zero for anything that will do; here it
     * simply refuses what cannot be stood on, which is what makes a wander stay on the ground.
     */
    private fun walkTargetValue(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Double {
        val ground = world.getBlockState(x, y - 1, z) ?: return Double.NEGATIVE_INFINITY
        if (BlockShapes.isEmptyShape(ground)) return Double.NEGATIVE_INFINITY
        val feet = world.getBlockState(x, y, z) ?: return Double.NEGATIVE_INFINITY
        val head = world.getBlockState(x, y + 1, z) ?: return Double.NEGATIVE_INFINITY
        if (!BlockShapes.isEmptyShape(feet) || !BlockShapes.isEmptyShape(head)) return Double.NEGATIVE_INFINITY
        return 0.0
    }

    /** parity: `MoveControl.tick`, which turns at most thirty degrees a tick towards the target. */
    private fun turnTowards(
        mob: ServerEntity,
        dx: Double,
        dz: Double,
    ) {
        val wanted = (atan2(dz, dx) * 180.0 / PI).toFloat() - 90f
        mob.yaw = approachAngle(mob.yaw, wanted, TURN_RATE)
    }

    /** parity: `Mth.approachDegrees`, the shortest way round, capped at [limit]. */
    private fun approachAngle(
        from: Float,
        to: Float,
        limit: Float,
    ): Float {
        var difference = (to - from) % 360f
        if (difference < -180f) difference += 360f
        if (difference > 180f) difference -= 360f
        return from + difference.coerceIn(-limit, limit)
    }

    /** How far a mob is from a point, for the goals that care. */
    public fun distanceTo(
        mob: ServerEntity,
        x: Double,
        y: Double,
        z: Double,
    ): Double {
        val dx = mob.x - x
        val dy = mob.y - y
        val dz = mob.z - z
        return sqrt(dx * dx + dy * dy + dz * dz)
    }
}
