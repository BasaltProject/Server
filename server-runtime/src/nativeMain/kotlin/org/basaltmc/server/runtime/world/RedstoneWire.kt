package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.wire.common.Identifier

/**
 * Which way a line of redstone dust runs.
 *
 * parity: `RedStoneWireBlock.getConnectionState`. A dust reaches towards whatever it can carry a
 * signal to, climbs one block when the way up is clear, and when it finds nothing on one axis it
 * straightens out along the other rather than sitting there as a dot.
 */
public object RedstoneWire {
    public val BLOCK: Identifier = Identifier.vanilla("redstone_wire")

    private const val NONE = "none"
    private const val SIDE = "side"
    private const val UP = "up"

    private val SIDES = listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.EAST, BlockFace.WEST)

    public fun isWire(stateId: Int?): Boolean = stateId != null && BlockStateDefinitions.blockOf(stateId) == BLOCK

    /** The shape the dust at these coordinates should have, written if it moved. Answers the state. */
    public fun reshape(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        if (!isWire(stateId)) return null
        val wanted = connectionsFor(world, x, y, z, stateId)
        val turned = BlockStateDefinitions.stateIdOf(BLOCK, wanted, stateId) ?: return null
        if (turned == stateId) return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return turned
    }

    /** parity: `getConnectionState`, including the "a dot stays a dot" clause. */
    public fun connectionsFor(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Map<String, String> {
        val was = BlockStateDefinitions.propertiesOf(stateId)
        val wasDot = SIDES.all { (was[it.name.lowercase()] ?: NONE) == NONE }
        // parity: `canConnectUp`, which is one question asked once: is the way up clear at all.
        val canGoUp = !conducts(world, x, y + 1, z)
        val sides = SIDES.associate { it.name.lowercase() to connectingSide(world, x, y, z, it, canGoUp) }
        if (wasDot && sides.values.all { it == NONE }) return sides
        val north = sides.getValue("north") != NONE
        val south = sides.getValue("south") != NONE
        val east = sides.getValue("east") != NONE
        val west = sides.getValue("west") != NONE
        val noZ = !north && !south
        val noX = !east && !west
        // parity: a dust with nothing on one axis draws itself straight along the other.
        val settled = sides.toMutableMap()
        if (!west && noZ) settled["west"] = SIDE
        if (!east && noZ) settled["east"] = SIDE
        if (!north && noX) settled["north"] = SIDE
        if (!south && noX) settled["south"] = SIDE
        return settled
    }

    /** parity: `getConnectingSide`, the one that decides side, up or nothing. */
    private fun connectingSide(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        face: BlockFace,
        canGoUp: Boolean,
    ): String {
        val nx = x + face.stepX
        val nz = z + face.stepZ
        val neighbour = world.state.getBlockState(nx, y, nz) ?: return NONE
        if (canGoUp) {
            val above = world.state.getBlockState(nx, y + 1, nz) ?: return NONE
            // parity: a trapdoor is the exception that a dust can always climb over.
            val climbable =
                BlockStateDefinitions.blockOf(neighbour)?.path?.endsWith("_trapdoor") == true ||
                    BlockSupport.canSurvive(
                        stateId = wireState(),
                        down = neighbour,
                        up = 0,
                        north = 0,
                        south = 0,
                        west = 0,
                        east = 0,
                        wet = false,
                    )
            if (climbable && connectsTo(above, null)) {
                return if (BlockShapes.isFaceSturdy(neighbour, face.opposite)) UP else SIDE
            }
        }
        if (connectsTo(neighbour, face)) return SIDE
        if (BlockSupport.conductsRedstone(neighbour)) return NONE
        val below = world.state.getBlockState(nx, y - 1, nz) ?: return NONE
        return if (connectsTo(below, null)) SIDE else NONE
    }

    /**
     * parity: `shouldConnectTo`: dust joins dust, a diode only end on, an observer only at its face,
     * and anything else that gives off a signal from a side.
     */
    private fun connectsTo(
        stateId: Int,
        from: BlockFace?,
    ): Boolean {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return false
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        return when (block.path) {
            "redstone_wire" -> true
            "repeater" -> {
                val facing = properties["facing"] ?: return false
                from != null && (facing == from.name.lowercase() || facing == from.opposite.name.lowercase())
            }
            "observer" -> from != null && properties["facing"] == from.name.lowercase()
            else -> from != null && Redstone.isSignalSource(stateId)
        }
    }

    private fun conducts(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean = BlockSupport.conductsRedstone(world.state.getBlockState(x, y, z) ?: return false)

    private fun wireState(): Int = BlockStateDefinitions.stateIdOf(BLOCK, emptyMap()) ?: 0
}
