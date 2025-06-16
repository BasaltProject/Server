package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.Direction
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin

/**
 * Which state a block goes down as: facing somewhere, on its side, upside down.
 *
 * parity: each block's own `getStateForPlacement`. The families are told apart by the properties
 * the block actually has, which comes from its real state definition, plus a list of the blocks
 * that break the family rule (a lightning rod points where it was clicked, a staircase looks the
 * same way the player does).
 *
 * What is not modelled: anything needing to read the world back. A ladder does not check the wall
 * behind it, an end rod put against another end rod does not flip, and no block refuses a spot it
 * cannot hold on to.
 */
public object BlockPlacement {
    /** Six-way blocks that point at the face that was clicked rather than away from the player. */
    private val CLICKED_FACE_BLOCKS =
        setOf("lightning_rod", "end_rod", "shulker_box", "amethyst_cluster", "amethyst_bud")

    /** Horizontal blocks that face the same way as the player instead of back at them. */
    private val PLAYER_FACING_BLOCKS = setOf("campfire", "bell", "decorated_pot", "calibrated_sculk_sensor")

    /**
     * The state [block] should be placed in, or [defaultStateId] when nothing about it orients.
     *
     * [clickY] is where in the clicked block the cursor was, from 0 at its floor to 1 at its
     * ceiling, and [replacing] the state already in the spot, which is what turns two slabs into a
     * double one.
     */
    public fun stateFor(
        block: Identifier,
        defaultStateId: Int,
        face: Direction,
        clickY: Float,
        yaw: Float,
        pitch: Float,
        replacing: Int,
    ): Int {
        val definition = BlockStateDefinitions.of(block) ?: return defaultStateId
        val properties = mutableMapOf<String, String>()

        if (definition.valuesOf("axis").size == 3) properties["axis"] = axisOf(face)

        // parity: `FaceAttachedHorizontalDirectionalBlock`, the buttons, levers and grindstones:
        // what they hang on comes from the face that was clicked, and only then which way they look.
        val attachment = definition.valuesOf("face")
        if (attachment.isNotEmpty()) {
            properties["face"] =
                when (face) {
                    Direction.UP -> "floor"
                    Direction.DOWN -> "ceiling"
                    else -> "wall"
                }
        }

        val facing = definition.valuesOf("facing")
        when {
            facing.size == 6 ->
                properties["facing"] =
                    if (isClickedFaceBlock(block)) name(face) else name(opposite(nearestLooking(yaw, pitch)))
            facing.size == 4 && attachment.isNotEmpty() ->
                properties["facing"] =
                    if (properties["face"] == "wall") name(face) else name(playerDirection(yaw))
            // parity: `AnvilBlock`, which lands sideways to whoever put it down.
            facing.size == 4 && block.path.endsWith("anvil") ->
                properties["facing"] =
                    name(clockwise(playerDirection(yaw)))
            facing.size == 4 ->
                properties["facing"] =
                    if (isPlayerFacingBlock(block)) name(playerDirection(yaw)) else name(opposite(playerDirection(yaw)))
        }

        // parity: the same test in `StairBlock`, `SlabBlock` and `TrapDoorBlock`: the top half only
        // when the click was on a ceiling, or above the middle of a side.
        val upsideDown = face == Direction.DOWN || (face != Direction.UP && clickY > HALF)
        if (definition.valuesOf("half").contains("top")) properties["half"] = if (upsideDown) "top" else "bottom"
        val type = definition.valuesOf("type")
        if (type.contains("double")) {
            properties["type"] =
                when {
                    BlockStateDefinitions.blockOf(replacing) == block -> "double"
                    upsideDown -> "top"
                    else -> "bottom"
                }
        }

        if (properties.isEmpty()) return defaultStateId
        return BlockStateDefinitions.stateIdOf(block, properties, fromStateId = defaultStateId) ?: defaultStateId
    }

    private fun isClickedFaceBlock(block: Identifier): Boolean =
        CLICKED_FACE_BLOCKS.any { block.path == it || block.path.endsWith("_$it") }

    private fun isPlayerFacingBlock(block: Identifier): Boolean =
        block.path.endsWith("_stairs") ||
            // A door is only half placed here (there is no second block), but it faces right.
            block.path.endsWith("_door") ||
            PLAYER_FACING_BLOCKS.any { block.path == it || block.path.endsWith("_$it") }

    /** parity: `Direction.fromYRot`, the quarter turn the player's yaw falls in. */
    public fun playerDirection(yaw: Float): Direction = HORIZONTAL[floor(yaw / QUARTER_TURN + 0.5f).toInt() and 3]

    /** parity: `Direction.orderedByNearest`'s first entry: whichever face the player looks most along. */
    public fun nearestLooking(
        yaw: Float,
        pitch: Float,
    ): Direction {
        val yawRadians = yaw * PI / HALF_TURN
        val pitchRadians = pitch * PI / HALF_TURN
        val x = -sin(yawRadians) * cos(pitchRadians)
        val y = -sin(pitchRadians)
        val z = cos(yawRadians) * cos(pitchRadians)
        return when {
            abs(y) >= abs(x) && abs(y) >= abs(z) -> if (y > 0) Direction.UP else Direction.DOWN
            abs(x) >= abs(z) -> if (x > 0) Direction.EAST else Direction.WEST
            else -> if (z > 0) Direction.SOUTH else Direction.NORTH
        }
    }

    private fun clockwise(face: Direction): Direction =
        when (face) {
            Direction.NORTH -> Direction.EAST
            Direction.EAST -> Direction.SOUTH
            Direction.SOUTH -> Direction.WEST
            Direction.WEST -> Direction.NORTH
            else -> face
        }

    private fun axisOf(face: Direction): String =
        when (face) {
            Direction.UP, Direction.DOWN -> "y"
            Direction.NORTH, Direction.SOUTH -> "z"
            else -> "x"
        }

    private fun name(face: Direction): String = face.name.lowercase()

    private fun opposite(face: Direction): Direction =
        when (face) {
            Direction.DOWN -> Direction.UP
            Direction.UP -> Direction.DOWN
            Direction.NORTH -> Direction.SOUTH
            Direction.SOUTH -> Direction.NORTH
            Direction.WEST -> Direction.EAST
            Direction.EAST -> Direction.WEST
        }

    /** South first, the way vanilla numbers the quarters of a yaw. */
    private val HORIZONTAL = arrayOf(Direction.SOUTH, Direction.WEST, Direction.NORTH, Direction.EAST)

    private const val QUARTER_TURN = 90f
    private const val HALF_TURN = 180.0
    private const val HALF = 0.5f
}
