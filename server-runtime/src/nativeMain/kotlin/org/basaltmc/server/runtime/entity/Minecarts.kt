package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * A minecart on a rail.
 *
 * parity: `OldMinecartBehavior.moveAlongTrack`. A cart does not move freely: it is pinned to the
 * line between the rail's two exits and its speed is projected onto that line.
 */
public object Minecarts {
    /** parity: `getMaxSpeed`, 0.4 on land and 0.2 in water. */
    public const val MAX_SPEED: Double = 0.4

    /** parity: `getSlowdownFactor`: a cart with somebody in it keeps far more of its speed. */
    public const val SLOWDOWN: Double = 0.96
    public const val RIDDEN_SLOWDOWN: Double = 0.997

    /** parity: the 0.75 a ridden cart's own movement is scaled by. */
    private const val RIDDEN_SCALE = 0.75

    /** parity: the nudge a rider leaning on a key gives a cart that is barely moving. */
    private const val RIDER_PUSH = 0.001
    private const val RIDER_PUSH_LIMIT = 0.01

    /** parity: `AbstractMinecart.push`, the shove a cart takes from whoever walks into it. */
    private const val PUSH_STRENGTH = 0.1
    private const val PUSH_HALF = 0.5
    private const val PUSH_EPSILON = 1.0E-4

    /** parity: the `slideSpeed` a slope is worth. */
    private const val SLOPE_PUSH = 0.0078125

    /** parity: what a powered rail adds, and the nudge it gives a cart standing still. */
    private const val BOOST = 0.06
    private const val NUDGE = 0.02

    /** parity: `getPos`'s own sixteenth of a block, and the twentieth of the drop it turns into speed. */
    private const val RAIL_LIFT = 0.0625
    private const val SLOPE_SPEED = 0.05

    /** parity: `comeOffTrack`, which halves the speed on the ground and takes 0.95 of it in the air. */
    private const val GROUND_DRAG = 0.5
    private const val AIR_DRAG = 0.95

    /** parity: under this, an unpowered powered-rail stops the cart dead. */
    private const val HALT_SPEED = 0.03

    /** parity: `AbstractMinecart.EXITS`, the two ways off each rail shape. */
    private val EXITS: Map<String, Pair<IntArray, IntArray>> =
        mapOf(
            "north_south" to (intArrayOf(0, 0, -1) to intArrayOf(0, 0, 1)),
            "east_west" to (intArrayOf(-1, 0, 0) to intArrayOf(1, 0, 0)),
            "ascending_east" to (intArrayOf(-1, -1, 0) to intArrayOf(1, 0, 0)),
            "ascending_west" to (intArrayOf(-1, 0, 0) to intArrayOf(1, -1, 0)),
            "ascending_north" to (intArrayOf(0, 0, -1) to intArrayOf(0, -1, 1)),
            "ascending_south" to (intArrayOf(0, -1, -1) to intArrayOf(0, 0, 1)),
            "south_east" to (intArrayOf(0, 0, 1) to intArrayOf(1, 0, 0)),
            "south_west" to (intArrayOf(0, 0, 1) to intArrayOf(-1, 0, 0)),
            "north_west" to (intArrayOf(0, 0, -1) to intArrayOf(-1, 0, 0)),
            "north_east" to (intArrayOf(0, 0, -1) to intArrayOf(1, 0, 0)),
        )

    private val RAILS = setOf("rail", "powered_rail", "detector_rail", "activator_rail")

    /** The rail the cart is on: its own block, or the one under it. parity: `getCurrentBlockPosOrRailBelow`. */
    public fun railUnder(
        world: WorldState,
        cart: ServerEntity,
    ): IntArray? {
        val x = floor(cart.x).toInt()
        val y = floor(cart.y).toInt()
        val z = floor(cart.z).toInt()
        if (isRail(world, x, y, z)) return intArrayOf(x, y, z)
        if (isRail(world, x, y - 1, z)) return intArrayOf(x, y - 1, z)
        return null
    }

    private fun isRail(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockStateDefinitions.blockOf(world.getBlockState(x, y, z) ?: return false)?.path in RAILS

    /**
     * One tick on a rail. Answers false when there is no rail under the cart, which is when the
     * caller lets it fall instead.
     */
    public fun moveAlongTrack(
        world: WorldState,
        cart: ServerEntity,
        intentX: Double = 0.0,
        intentZ: Double = 0.0,
        ridden: Boolean = false,
    ): Boolean {
        val rail = railUnder(world, cart) ?: return false
        val stateId = world.getBlockState(rail[0], rail[1], rail[2]) ?: return false
        // parity: the height the cart is at *on the rail* before it moves, which is what tells a
        // slope from level ground once it has moved.
        val oldRailY = railPosition(world, cart.x, cart.y, cart.z)?.get(1)
        val block = BlockStateDefinitions.blockOf(stateId) ?: return false
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val shape = properties["shape"] ?: return false
        val (exitA, exitB) = EXITS[shape] ?: return false

        var powered = false
        var halting = false
        if (block.path == "powered_rail") {
            powered = properties["powered"] == "true"
            halting = !powered
        }

        // parity: a slope pushes the cart downhill before anything else happens.
        when (shape) {
            "ascending_east" -> cart.velocityX -= SLOPE_PUSH
            "ascending_west" -> cart.velocityX += SLOPE_PUSH
            "ascending_north" -> cart.velocityZ += SLOPE_PUSH
            "ascending_south" -> cart.velocityZ -= SLOPE_PUSH
        }
        val railY = if (shape.startsWith("ascending")) rail[1] + 1 else rail[1]

        // parity: the speed is projected onto the rail, keeping its size and taking its direction.
        var alongX = (exitB[0] - exitA[0]).toDouble()
        var alongZ = (exitB[2] - exitA[2]).toDouble()
        val length = sqrt(alongX * alongX + alongZ * alongZ)
        if (cart.velocityX * alongX + cart.velocityZ * alongZ < 0.0) {
            alongX = -alongX
            alongZ = -alongZ
        }
        val speed = min(2.0, sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ))
        cart.velocityX = speed * alongX / length
        cart.velocityZ = speed * alongZ / length

        // parity: a rider leaning on a key gets a stopped cart going again, which is also what
        // stops an unpowered powered rail from holding it there.
        if ((intentX != 0.0 || intentZ != 0.0) &&
            cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ < RIDER_PUSH_LIMIT
        ) {
            val length = sqrt(intentX * intentX + intentZ * intentZ)
            if (length > 0.0) {
                cart.velocityX += intentX / length * RIDER_PUSH
                cart.velocityZ += intentZ / length * RIDER_PUSH
                halting = false
            }
        }
        // parity: an unpowered powered rail is a brake, and stops the cart under 0.03.
        if (halting) {
            val moving = sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ)
            if (moving < HALT_SPEED) {
                cart.velocityX = 0.0
                cart.velocityZ = 0.0
            } else {
                cart.velocityX *= 0.5
                cart.velocityZ *= 0.5
            }
        }

        // parity: the cart is pinned to the line between the two exits rather than moving freely.
        val fromX = rail[0] + 0.5 + exitA[0] * 0.5
        val fromZ = rail[2] + 0.5 + exitA[2] * 0.5
        val toX = rail[0] + 0.5 + exitB[0] * 0.5
        val toZ = rail[2] + 0.5 + exitB[2] * 0.5
        val spanX = toX - fromX
        val spanZ = toZ - fromZ
        val progress =
            when {
                spanX == 0.0 -> cart.z - rail[2]
                spanZ == 0.0 -> cart.x - rail[0]
                else -> ((cart.x - fromX) * spanX + (cart.z - fromZ) * spanZ) * 2.0
            }
        cart.x = fromX + spanX * progress
        cart.y = railY.toDouble()
        cart.z = fromZ + spanZ * progress

        val scale = if (ridden) RIDDEN_SCALE else 1.0
        cart.x += (scale * cart.velocityX).coerceIn(-MAX_SPEED, MAX_SPEED)
        cart.z += (scale * cart.velocityZ).coerceIn(-MAX_SPEED, MAX_SPEED)

        // parity: leaving the rail's own block down a slope drops the cart onto the next rail.
        val steppedX = floor(cart.x).toInt() - rail[0]
        val steppedZ = floor(cart.z).toInt() - rail[2]
        if (exitA[1] != 0 && steppedX == exitA[0] && steppedZ == exitA[2]) {
            cart.y += exitA[1]
        } else if (exitB[1] != 0 && steppedX == exitB[0] && steppedZ == exitB[2]) {
            cart.y += exitB[1]
        }

        // parity: `applyNaturalSlowdown`, the 0.96 a riderless cart keeps, and the y it drops.
        val slowdown = if (ridden) RIDDEN_SLOWDOWN else SLOWDOWN
        cart.velocityX *= slowdown
        cart.velocityZ *= slowdown
        cart.velocityY = 0.0

        // parity: the twentieth of the height it lost, added to its speed. This is what makes a
        // cart run away downhill and crawl uphill instead of rolling at one speed everywhere.
        val newRail = railPosition(world, cart.x, cart.y, cart.z)
        if (oldRailY != null && newRail != null) {
            val gained = (oldRailY - newRail[1]) * SLOPE_SPEED
            val moving = sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ)
            if (moving > 0.0) {
                cart.velocityX *= (moving + gained) / moving
                cart.velocityZ *= (moving + gained) / moving
            }
            cart.y = newRail[1]
        }

        // parity: a cart that crossed into the next block points its speed the way it stepped.
        val steppedIntoX = floor(cart.x).toInt()
        val steppedIntoZ = floor(cart.z).toInt()
        if (steppedIntoX != rail[0] || steppedIntoZ != rail[2]) {
            val moving = sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ)
            cart.velocityX = moving * (steppedIntoX - rail[0])
            cart.velocityZ = moving * (steppedIntoZ - rail[2])
        }

        if (powered) boost(world, cart, rail, shape)
        cart.onGround = true
        return true
    }

    /**
     * Exactly where on the rail a point sits, height included, or null when there is no rail there.
     *
     * parity: `OldMinecartBehavior.getPos`, whose sixteenth of a block of lift and its half-block
     * step on a slope are what the cart actually rides on.
     */
    public fun railPosition(
        world: WorldState,
        x: Double,
        y: Double,
        z: Double,
    ): DoubleArray? {
        val blockX = floor(x).toInt()
        var blockY = floor(y).toInt()
        val blockZ = floor(z).toInt()
        if (isRail(world, blockX, blockY - 1, blockZ)) blockY--
        val stateId = world.getBlockState(blockX, blockY, blockZ) ?: return null
        if (BlockStateDefinitions.blockOf(stateId)?.path !in RAILS) return null
        val shape = BlockStateDefinitions.propertiesOf(stateId)["shape"] ?: return null
        val (exitA, exitB) = EXITS[shape] ?: return null
        val fromX = blockX + 0.5 + exitA[0] * 0.5
        val fromY = blockY + RAIL_LIFT + exitA[1] * 0.5
        val fromZ = blockZ + 0.5 + exitA[2] * 0.5
        val spanX = blockX + 0.5 + exitB[0] * 0.5 - fromX
        val spanY = (blockY + RAIL_LIFT + exitB[1] * 0.5 - fromY) * 2.0
        val spanZ = blockZ + 0.5 + exitB[2] * 0.5 - fromZ
        val progress =
            when {
                spanX == 0.0 -> z - blockZ
                spanZ == 0.0 -> x - blockX
                else -> ((x - fromX) * spanX + (z - fromZ) * spanZ) * 2.0
            }
        var height = fromY + spanY * progress
        if (spanY < 0.0) {
            height += 1.0
        } else if (spanY > 0.0) {
            height += 0.5
        }
        return doubleArrayOf(fromX + spanX * progress, height, fromZ + spanZ * progress)
    }

    /**
     * One tick off the rails.
     *
     * parity: `AbstractMinecart.comeOffTrack`: the speed is capped, halved while it is on the
     * ground, and what is left of it is air drag.
     */
    public fun comeOffTrack(
        world: WorldState,
        cart: ServerEntity,
        gravity: Double,
    ) {
        cart.velocityX = cart.velocityX.coerceIn(-MAX_SPEED, MAX_SPEED)
        cart.velocityZ = cart.velocityZ.coerceIn(-MAX_SPEED, MAX_SPEED)
        if (cart.onGround) {
            cart.velocityX *= GROUND_DRAG
            cart.velocityY *= GROUND_DRAG
            cart.velocityZ *= GROUND_DRAG
        }
        EntityPhysics.move(world, cart, gravity = gravity, drag = 1.0)
        if (!cart.onGround) {
            cart.velocityX *= AIR_DRAG
            cart.velocityY *= AIR_DRAG
            cart.velocityZ *= AIR_DRAG
        }
    }

    /** parity: a powered rail adds 0.06 along the way it is going, or nudges a still cart off a wall. */
    private fun boost(
        world: WorldState,
        cart: ServerEntity,
        rail: IntArray,
        shape: String,
    ) {
        val moving = sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ)
        if (moving > 0.01) {
            cart.velocityX += cart.velocityX / moving * BOOST
            cart.velocityZ += cart.velocityZ / moving * BOOST
            return
        }
        when (shape) {
            "east_west" ->
                when {
                    isSolid(world, rail[0] - 1, rail[1], rail[2]) -> cart.velocityX = NUDGE
                    isSolid(world, rail[0] + 1, rail[1], rail[2]) -> cart.velocityX = -NUDGE
                }
            "north_south" ->
                when {
                    isSolid(world, rail[0], rail[1], rail[2] - 1) -> cart.velocityZ = NUDGE
                    isSolid(world, rail[0], rail[1], rail[2] + 1) -> cart.velocityZ = -NUDGE
                }
        }
    }

    private fun isSolid(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockShapes.isFullCube(world.getBlockState(x, y, z) ?: return false)

    /**
     * The direction the keys a rider is holding point, in the world.
     *
     * parity: `ServerPlayer.getLastClientMoveIntent` over `Entity.getInputVector`: the keys make a
     * vector, it is normalised, and the player's own yaw turns it.
     */
    public fun intentOf(
        forward: Boolean,
        back: Boolean,
        left: Boolean,
        right: Boolean,
        yaw: Float,
    ): DoubleArray {
        val sideways = (if (left) 1.0 else 0.0) - (if (right) 1.0 else 0.0)
        val ahead = (if (forward) 1.0 else 0.0) - (if (back) 1.0 else 0.0)
        val length = sqrt(sideways * sideways + ahead * ahead)
        if (length == 0.0) return doubleArrayOf(0.0, 0.0)
        val x = sideways / length
        val z = ahead / length
        val radians = yaw * PI / DEGREES_PER_RADIAN
        val sin = sin(radians)
        val cos = cos(radians)
        return doubleArrayOf(x * cos - z * sin, z * cos + x * sin)
    }

    /**
     * The shove a cart takes from somebody walking into it.
     *
     * parity: `AbstractMinecart.push`, which pushes the cart away from whoever touched it at a
     * twentieth of the distance between them, and never harder than that.
     */
    public fun pushedBy(
        cart: ServerEntity,
        fromX: Double,
        fromZ: Double,
    ) {
        var dx = fromX - cart.x
        var dz = fromZ - cart.z
        val squared = dx * dx + dz * dz
        if (squared < PUSH_EPSILON) return
        val distance = sqrt(squared)
        dx /= distance
        dz /= distance
        val strength = minOf(1.0 / distance, 1.0) * PUSH_STRENGTH * PUSH_HALF
        cart.velocityX -= dx * strength
        cart.velocityZ -= dz * strength
    }

    private const val DEGREES_PER_RADIAN = 180.0

    /** How fast the cart is going, for the callers that only care about that. */
    public fun speedOf(cart: ServerEntity): Double = abs(cart.velocityX) + abs(cart.velocityZ)
}
