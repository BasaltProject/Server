package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions

/**
 * How a rail bends towards the rails around it.
 *
 * parity: `RailState`. A rail keeps at most two connections and looks one block up and one down for
 * them; only a plain rail turns a corner, the three that carry a signal may just climb.
 */
public object Rails {
    /** The four shapes a rail that cannot turn is allowed to take, plus its two slopes. */
    private val STRAIGHT_ONLY = setOf("powered_rail", "detector_rail", "activator_rail")

    private val RAILS = STRAIGHT_ONLY + "rail"

    private const val NORTH_SOUTH = "north_south"
    private const val EAST_WEST = "east_west"

    public fun isRail(stateId: Int?): Boolean = stateId != null && BlockStateDefinitions.blockOf(stateId)?.path in RAILS

    /**
     * Bends the rail at these coordinates towards its neighbours, and them towards it.
     *
     * parity: `RailState.place` with `first = true`, which is what `BaseRailBlock.onPlace` runs.
     * Answers the shape it settled on, or null when there is no rail there.
     */
    public fun place(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        changed: (Int, Int, Int, Int) -> Unit = { _, _, _, _ -> },
    ): Int? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (!isRail(stateId)) return null
        val shape = shapeFor(world, x, y, z, stateId) ?: return null
        val turned = withShape(world, x, y, z, stateId, shape) ?: return null
        changed(x, y, z, turned)
        // parity: `place` re-bends every rail it decided to connect to.
        for (at in connectionsOf(x, y, z, shape)) {
            val neighbour = railAt(world, at[0], at[1], at[2]) ?: continue
            val neighbourState = world.state.getBlockState(neighbour[0], neighbour[1], neighbour[2]) ?: continue
            if (!canConnectTo(world, neighbour, x, y, z)) continue
            connectTo(world, neighbour, neighbourState, x, z, changed)
        }
        return turned
    }

    /**
     * The rail's own shape, worked out again without touching its neighbours.
     *
     * parity: `BaseRailBlock.updateShape`. The neighbours get their own turn from the update that is
     * already walking them, so bending them from here would write each of them twice.
     */
    public fun reshape(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Int? {
        if (!isRail(stateId)) return null
        val shape = shapeFor(world, x, y, z, stateId) ?: return null
        if (BlockStateDefinitions.propertiesOf(stateId)["shape"] == shape) return null
        return withShape(world, x, y, z, stateId, shape)
    }

    /** parity: `RailState.place`'s shape choice, neighbours first and the default only as a fallback. */
    private fun shapeFor(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): String? {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val defaultShape = properties["shape"] ?: return null
        val straight = BlockStateDefinitions.blockOf(stateId)?.path in STRAIGHT_ONLY
        val north = hasNeighbourRail(world, x, y, z, 0, -1)
        val south = hasNeighbourRail(world, x, y, z, 0, 1)
        val west = hasNeighbourRail(world, x, y, z, -1, 0)
        val east = hasNeighbourRail(world, x, y, z, 1, 0)
        val alongZ = north || south
        val alongX = west || east
        var shape: String? = null
        if (alongZ && !alongX) shape = NORTH_SOUTH
        if (alongX && !alongZ) shape = EAST_WEST
        if (!straight) {
            if (south && east && !north && !west) shape = "south_east"
            if (south && west && !north && !east) shape = "south_west"
            if (north && west && !south && !east) shape = "north_west"
            if (north && east && !south && !west) shape = "north_east"
        }
        if (shape == null) {
            shape =
                when {
                    alongZ && alongX -> defaultShape
                    alongZ -> NORTH_SOUTH
                    alongX -> EAST_WEST
                    else -> null
                }
            // parity: with no signal the corners are tried in this order, last one winning.
            if (!straight) {
                if (north && west) shape = "north_west"
                if (north && east) shape = "north_east"
                if (south && west) shape = "south_west"
                if (south && east) shape = "south_east"
            }
        }
        return slope(world, x, y, z, shape ?: defaultShape)
    }

    /** parity: a straight rail climbs when the rail it points at is one block higher. */
    private fun slope(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        shape: String,
    ): String {
        var result = shape
        if (shape == NORTH_SOUTH) {
            if (isRail(world.state.getBlockState(x, y + 1, z - 1))) result = "ascending_north"
            if (isRail(world.state.getBlockState(x, y + 1, z + 1))) result = "ascending_south"
        }
        if (shape == EAST_WEST) {
            if (isRail(world.state.getBlockState(x + 1, y + 1, z))) result = "ascending_east"
            if (isRail(world.state.getBlockState(x - 1, y + 1, z))) result = "ascending_west"
        }
        return result
    }

    /** parity: `RailState.connectTo`, which bends a neighbour towards a rail that just appeared. */
    private fun connectTo(
        world: LoadedWorld,
        at: IntArray,
        stateId: Int,
        towardsX: Int,
        towardsZ: Int,
        changed: (Int, Int, Int, Int) -> Unit,
    ) {
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val shape = properties["shape"] ?: return
        val straight = BlockStateDefinitions.blockOf(stateId)?.path in STRAIGHT_ONLY
        val (x, y, z) = Triple(at[0], at[1], at[2])
        val links = softConnections(world, x, y, z, shape).toMutableList()
        links += intArrayOf(towardsX, y, towardsZ)
        val north = links.any { it[0] == x && it[2] == z - 1 }
        val south = links.any { it[0] == x && it[2] == z + 1 }
        val west = links.any { it[0] == x - 1 && it[2] == z }
        val east = links.any { it[0] == x + 1 && it[2] == z }
        var wanted: String? = null
        if (north || south) wanted = NORTH_SOUTH
        if (west || east) wanted = EAST_WEST
        if (!straight) {
            if (south && east && !north && !west) wanted = "south_east"
            if (south && west && !north && !east) wanted = "south_west"
            if (north && west && !south && !east) wanted = "north_west"
            if (north && east && !south && !west) wanted = "north_east"
        }
        val settled = slope(world, x, y, z, wanted ?: NORTH_SOUTH)
        withShape(world, x, y, z, stateId, settled)?.let { changed(x, y, z, it) }
    }

    /** parity: `canConnectTo`: a rail already joined at both ends takes no more. */
    private fun canConnectTo(
        world: LoadedWorld,
        at: IntArray,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val stateId = world.state.getBlockState(at[0], at[1], at[2]) ?: return false
        val shape = BlockStateDefinitions.propertiesOf(stateId)["shape"] ?: return false
        val links = softConnections(world, at[0], at[1], at[2], shape)
        if (links.any { it[0] == x && it[2] == z }) return true
        return links.size != 2
    }

    /**
     * parity: `removeSoftConnections`: a connection only counts when there is a rail there *and* it
     * points back. A rail pointing at empty ground still says it does in its own state, and taking
     * that at face value is what stops a corner ever forming.
     */
    private fun softConnections(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        shape: String,
    ): List<IntArray> =
        connectionsOf(x, y, z, shape).mapNotNull { towards ->
            val rail = railAt(world, towards[0], towards[1], towards[2]) ?: return@mapNotNull null
            val theirState = world.state.getBlockState(rail[0], rail[1], rail[2]) ?: return@mapNotNull null
            val theirShape = BlockStateDefinitions.propertiesOf(theirState)["shape"] ?: return@mapNotNull null
            val pointsBack =
                connectionsOf(rail[0], rail[1], rail[2], theirShape).any { it[0] == x && it[2] == z }
            if (pointsBack) rail else null
        }

    /** parity: `hasNeighborRail`, which is a rail that can still take one more connection. */
    private fun hasNeighbourRail(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stepX: Int,
        stepZ: Int,
    ): Boolean {
        val at = railAt(world, x + stepX, y, z + stepZ) ?: return false
        return canConnectTo(world, at, x, y, z)
    }

    /** parity: `getRail`, which looks at the block, then one up, then one down. */
    private fun railAt(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): IntArray? {
        if (isRail(world.state.getBlockState(x, y, z))) return intArrayOf(x, y, z)
        if (isRail(world.state.getBlockState(x, y + 1, z))) return intArrayOf(x, y + 1, z)
        if (isRail(world.state.getBlockState(x, y - 1, z))) return intArrayOf(x, y - 1, z)
        return null
    }

    /** parity: `updateConnections`, the two positions each shape reaches towards. */
    private fun connectionsOf(
        x: Int,
        y: Int,
        z: Int,
        shape: String,
    ): List<IntArray> =
        when (shape) {
            NORTH_SOUTH -> listOf(intArrayOf(x, y, z - 1), intArrayOf(x, y, z + 1))
            EAST_WEST -> listOf(intArrayOf(x - 1, y, z), intArrayOf(x + 1, y, z))
            "ascending_east" -> listOf(intArrayOf(x - 1, y, z), intArrayOf(x + 1, y + 1, z))
            "ascending_west" -> listOf(intArrayOf(x - 1, y + 1, z), intArrayOf(x + 1, y, z))
            "ascending_north" -> listOf(intArrayOf(x, y + 1, z - 1), intArrayOf(x, y, z + 1))
            "ascending_south" -> listOf(intArrayOf(x, y, z - 1), intArrayOf(x, y + 1, z + 1))
            "south_east" -> listOf(intArrayOf(x + 1, y, z), intArrayOf(x, y, z + 1))
            "south_west" -> listOf(intArrayOf(x - 1, y, z), intArrayOf(x, y, z + 1))
            "north_west" -> listOf(intArrayOf(x - 1, y, z), intArrayOf(x, y, z - 1))
            "north_east" -> listOf(intArrayOf(x + 1, y, z), intArrayOf(x, y, z - 1))
            else -> emptyList()
        }

    private fun withShape(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        shape: String,
    ): Int? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val turned = BlockStateDefinitions.stateIdOf(block, mapOf("shape" to shape), stateId) ?: return null
        if (turned == stateId) return stateId
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return turned
    }
}
