package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions

/**
 * How a wall stands: which sides it reaches out to, how high, and whether it keeps its post.
 *
 * parity: `WallBlock.updateShape` over `updateSides` and `shouldRaisePost`. The two questions about
 * what sits on top are shape tests, and they are asked of the real shapes: a wall under a full
 * block grows its sides tall, and a wall in a straight line loses its post unless something covers
 * the middle.
 */
public object Walls {
    private const val NONE = "none"
    private const val LOW = "low"
    private const val TALL = "tall"

    /**
     * parity: `WallBlock.TEST_SHAPE_POST` and `TEST_SHAPES_WALL`, in units of a thirty-second: the
     * middle two-sixteenths column, and a two-wide strip from each edge to the middle.
     */
    private const val MIDDLE_FROM = 14
    private const val MIDDLE_TO = 18
    private const val HALF = 18
    private const val FULL = 32

    /** The four sides and the post, as the properties a wall state carries. */
    public fun shapeOf(
        stateId: Int,
        north: Int,
        south: Int,
        west: Int,
        east: Int,
        above: Int,
    ): Map<String, String>? {
        if (!BlockConnections.isWall(stateId)) return null
        val sides =
            mapOf(
                "north" to side(connects(north, BlockFace.NORTH), above, BlockFace.NORTH),
                "south" to side(connects(south, BlockFace.SOUTH), above, BlockFace.SOUTH),
                "west" to side(connects(west, BlockFace.WEST), above, BlockFace.WEST),
                "east" to side(connects(east, BlockFace.EAST), above, BlockFace.EAST),
            )
        return sides + ("up" to raisesPost(sides, above).toString())
    }

    /**
     * parity: `WallBlock.connectsTo`, which is the fence rule plus every wall and every pane,
     * whether or not their faces are sturdy.
     */
    private fun connects(
        neighbour: Int,
        face: BlockFace,
    ): Boolean =
        BlockConnections.isWall(neighbour) ||
            BlockConnections.isPanes(neighbour) ||
            BlockConnections.connectsTo(WALL_LIKE_FENCE, neighbour, face)

    private fun side(
        connected: Boolean,
        above: Int,
        face: BlockFace,
    ): String {
        if (!connected) return NONE
        val covered =
            when (face) {
                BlockFace.NORTH -> BlockShapes.faceCovers(above, BlockFace.DOWN, 0, HALF, MIDDLE_FROM, MIDDLE_TO)
                BlockFace.SOUTH ->
                    BlockShapes.faceCovers(
                        above,
                        BlockFace.DOWN,
                        MIDDLE_FROM,
                        FULL,
                        MIDDLE_FROM,
                        MIDDLE_TO,
                    )
                BlockFace.WEST -> BlockShapes.faceCovers(above, BlockFace.DOWN, MIDDLE_FROM, MIDDLE_TO, 0, HALF)
                else -> BlockShapes.faceCovers(above, BlockFace.DOWN, MIDDLE_FROM, MIDDLE_TO, MIDDLE_FROM, FULL)
            }
        return if (covered) TALL else LOW
    }

    /** parity: `WallBlock.shouldRaisePost`: a corner always keeps its post, a straight run may not. */
    private fun raisesPost(
        sides: Map<String, String>,
        above: Int,
    ): Boolean {
        if (BlockConnections.isWall(above) && BlockStateDefinitions.propertiesOf(above)["up"] == "true") return true
        val northNone = sides.getValue("north") == NONE
        val southNone = sides.getValue("south") == NONE
        val westNone = sides.getValue("west") == NONE
        val eastNone = sides.getValue("east") == NONE
        val corner =
            (northNone && southNone && westNone && eastNone) || northNone != southNone || westNone != eastNone
        if (corner) return true
        val tallLine =
            (sides.getValue("north") == TALL && sides.getValue("south") == TALL) ||
                (sides.getValue("east") == TALL && sides.getValue("west") == TALL)
        if (tallLine) return false
        return BlockShapes.faceCovers(above, BlockFace.DOWN, MIDDLE_FROM, MIDDLE_TO, MIDDLE_FROM, MIDDLE_TO)
    }

    /**
     * A cobblestone wall's own state, used only to borrow the fence rule in [connects].
     *
     * The rule it borrows asks what the *neighbour* is, so which wall this is does not matter.
     */
    private val WALL_LIKE_FENCE: Int by lazy {
        BlockStateDefinitions.stateIdOf(
            org.basaltmc.wire.common.Identifier
                .vanilla("oak_fence"),
            emptyMap(),
        ) ?: 0
    }
}
