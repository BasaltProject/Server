package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier

/**
 * The rails that carry their own power along the line.
 *
 * parity: `PoweredRailBlock.updateState`. A powered or activator rail is on when something powers it
 * directly, or when a rail of its own kind up to eight along is on and *that* one is powered
 * directly. That is what makes one lever light a whole run of them, and why the ninth is dark.
 */
public object PoweredRails {
    /** parity: `findPoweredRailSignal`'s own eight, counted from the rail being asked about. */
    private const val SEARCH_LIMIT = 8

    private val KINDS = setOf("powered_rail", "activator_rail")

    /** Answers the change if the rail turned on or off, which is what the caller sends out. */
    public fun update(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockChange? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        if (block.path !in KINDS) return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val powered = properties["powered"] == "true"
        val wanted =
            Redstone.bestNeighbourSignal(world, x, y, z) > 0 ||
                findSignal(world, x, y, z, block, forward = true, depth = 0) ||
                findSignal(world, x, y, z, block, forward = false, depth = 0)
        if (wanted == powered) return null
        val turned =
            BlockStateDefinitions.stateIdOf(block, mapOf("powered" to wanted.toString()), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, turned)) return null
        return BlockChange(x, y, z, turned)
    }

    /**
     * parity: `findPoweredRailSignal`, which walks one rail along the line it is pointing and looks
     * one below as well, because a slope leaves the next rail a block down.
     */
    private fun findSignal(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        block: Identifier,
        forward: Boolean,
        depth: Int,
    ): Boolean {
        if (depth >= SEARCH_LIMIT) return false
        val shape = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: return false)["shape"]
        var nextX = x
        var nextY = y
        var nextZ = z
        var checkBelow = true
        val axis: String
        when (shape) {
            "north_south" -> {
                if (forward) nextZ++ else nextZ--
                axis = "north_south"
            }
            "east_west" -> {
                if (forward) nextX-- else nextX++
                axis = "east_west"
            }
            "ascending_east" -> {
                if (forward) {
                    nextX--
                } else {
                    nextX++
                    nextY++
                    checkBelow = false
                }
                axis = "east_west"
            }
            "ascending_west" -> {
                if (forward) {
                    nextX--
                    nextY++
                    checkBelow = false
                } else {
                    nextX++
                }
                axis = "east_west"
            }
            "ascending_north" -> {
                if (forward) {
                    nextZ++
                } else {
                    nextZ--
                    nextY++
                    checkBelow = false
                }
                axis = "north_south"
            }
            "ascending_south" -> {
                if (forward) {
                    nextZ++
                    nextY++
                    checkBelow = false
                } else {
                    nextZ--
                }
                axis = "north_south"
            }
            else -> return false
        }
        if (samePoweredRail(world, nextX, nextY, nextZ, block, forward, depth, axis)) return true
        return checkBelow && samePoweredRail(world, nextX, nextY - 1, nextZ, block, forward, depth, axis)
    }

    /**
     * parity: `isSameRailWithPower`. A rail that crosses the line rather than continuing it stops the
     * search, and one that is on but powered by nothing of its own keeps it going.
     */
    private fun samePoweredRail(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        block: Identifier,
        forward: Boolean,
        depth: Int,
        axis: String,
    ): Boolean {
        val stateId = world.state.getBlockState(x, y, z) ?: return false
        if (BlockStateDefinitions.blockOf(stateId) != block) return false
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val shape = properties["shape"] ?: return false
        if (axis == "east_west" && shape in NORTH_SOUTH_SHAPES) return false
        if (axis == "north_south" && shape in EAST_WEST_SHAPES) return false
        if (properties["powered"] != "true") return false
        if (Redstone.bestNeighbourSignal(world, x, y, z) > 0) return true
        return findSignal(world, x, y, z, block, forward, depth + 1)
    }

    private val NORTH_SOUTH_SHAPES = setOf("north_south", "ascending_north", "ascending_south")
    private val EAST_WEST_SHAPES = setOf("east_west", "ascending_east", "ascending_west")
}
