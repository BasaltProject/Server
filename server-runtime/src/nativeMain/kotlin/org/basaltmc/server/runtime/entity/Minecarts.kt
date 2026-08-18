package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.abs
import kotlin.math.floor
import kotlin.math.min
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

    /** parity: `getSlowdownFactor` with nobody riding. */
    public const val SLOWDOWN: Double = 0.96

    /** parity: the `slideSpeed` a slope is worth. */
    private const val SLOPE_PUSH = 0.0078125

    /** parity: what a powered rail adds, and the nudge it gives a cart standing still. */
    private const val BOOST = 0.06
    private const val NUDGE = 0.02

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
    ): Boolean {
        val rail = railUnder(world, cart) ?: return false
        val stateId = world.getBlockState(rail[0], rail[1], rail[2]) ?: return false
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

        cart.x += cart.velocityX.coerceIn(-MAX_SPEED, MAX_SPEED)
        cart.z += cart.velocityZ.coerceIn(-MAX_SPEED, MAX_SPEED)

        // parity: leaving the rail's own block down a slope drops the cart onto the next rail.
        val steppedX = floor(cart.x).toInt() - rail[0]
        val steppedZ = floor(cart.z).toInt() - rail[2]
        if (exitA[1] != 0 && steppedX == exitA[0] && steppedZ == exitA[2]) {
            cart.y += exitA[1]
        } else if (exitB[1] != 0 && steppedX == exitB[0] && steppedZ == exitB[2]) {
            cart.y += exitB[1]
        }

        // parity: `applyNaturalSlowdown`, the 0.96 a riderless cart keeps.
        cart.velocityX *= SLOWDOWN
        cart.velocityZ *= SLOWDOWN
        cart.velocityY = 0.0

        if (powered) boost(world, cart, rail, shape)
        cart.onGround = true
        return true
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

    /** How fast the cart is going, for the callers that only care about that. */
    public fun speedOf(cart: ServerEntity): Double = abs(cart.velocityX) + abs(cart.velocityZ)
}
