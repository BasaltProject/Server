package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.world.WorldState
import kotlin.math.abs
import kotlin.math.sqrt

/**
 * A way through the blocks from one place to another.
 *
 * parity: `PathFinder` over `WalkNodeEvaluator`: A* over the positions a mob can stand in, with the
 * heuristic weighted by 1.5 as vanilla weights it, a step up of one block, a drop of at most three,
 * and diagonals only when both of the blocks beside them are open. What a mob does with the path is
 * `PathNavigation`'s business, and here the caller's.
 */
public object PathFinder {
    /** parity: `PathFinder`'s own 1.5 on the heuristic, which is what makes it take the direct way. */
    private const val HEURISTIC_WEIGHT = 1.5

    /** parity: `WalkNodeEvaluator`: one block up without jumping, and a drop it will take. */
    private const val MAX_DROP = 3

    /** How many nodes one search may open before it gives up. parity: `PathFinder`'s own 200. */
    private const val MAX_NODES = 200

    /** parity: `WalkNodeEvaluator.getPathTypeStatic`'s malus for water and what burns. */
    private const val WATER_MALUS = 8.0
    private const val DANGER_MALUS = 16.0

    /** The path as the blocks to walk through, from the first step to the target. */
    public fun find(
        world: WorldState,
        fromX: Int,
        fromY: Int,
        fromZ: Int,
        toX: Int,
        toY: Int,
        toZ: Int,
    ): List<IntArray>? {
        if (!canStandAt(world, fromX, fromY, fromZ)) return null
        val start = pack(fromX, fromY, fromZ)
        val goal = pack(toX, toY, toZ)
        if (start == goal) return emptyList()

        val cameFrom = HashMap<Long, Long>()
        val cost = HashMap<Long, Double>()
        val open = ArrayDeque<Long>()
        val score = HashMap<Long, Double>()
        cost[start] = 0.0
        score[start] = heuristic(fromX, fromY, fromZ, toX, toY, toZ)
        open.addLast(start)

        var examined = 0
        while (open.isNotEmpty() && examined++ < MAX_NODES) {
            // A plain scan for the cheapest open node: a path this short never has enough of them
            // for a heap to pay for itself.
            val current = open.minByOrNull { score[it] ?: Double.MAX_VALUE } ?: break
            open.remove(current)
            if (current == goal) return reconstruct(cameFrom, current)

            val x = unpackX(current)
            val y = unpackY(current)
            val z = unpackZ(current)
            for (neighbour in neighbours(world, x, y, z)) {
                val step = pack(neighbour[0], neighbour[1], neighbour[2])
                val stepCost =
                    (cost[current] ?: Double.MAX_VALUE) +
                        distance(x, y, z, neighbour[0], neighbour[1], neighbour[2]) +
                        malus(world, neighbour[0], neighbour[1], neighbour[2])
                if (stepCost >= (cost[step] ?: Double.MAX_VALUE)) continue
                cameFrom[step] = current
                cost[step] = stepCost
                score[step] =
                    stepCost + heuristic(neighbour[0], neighbour[1], neighbour[2], toX, toY, toZ) * HEURISTIC_WEIGHT
                if (step !in open) open.addLast(step)
            }
        }
        return null
    }

    /** parity: `WalkNodeEvaluator.getNeighbors`: the four sides and the four corners, up one or down three. */
    private fun neighbours(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): List<IntArray> {
        val found = mutableListOf<IntArray>()
        for (face in HORIZONTAL) {
            val nx = x + face.stepX
            val nz = z + face.stepZ
            step(world, nx, y, nz)?.let { found += it }
        }
        // parity: a corner is only walkable when both sides of it are, which is what stops a mob
        // cutting through the inside of a corner.
        for (dx in listOf(-1, 1)) {
            for (dz in listOf(-1, 1)) {
                if (!isOpen(world, x + dx, y, z) || !isOpen(world, x, y, z + dz)) continue
                step(world, x + dx, y, z + dz)?.let { found += it }
            }
        }
        return found
    }

    /** Where a step in this direction lands: the same height, one up, or up to three down. */
    private fun step(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): IntArray? {
        if (canStandAt(world, x, y, z)) return intArrayOf(x, y, z)
        if (canStandAt(world, x, y + 1, z) && isOpen(world, x, y + 2, z)) return intArrayOf(x, y + 1, z)
        for (drop in 1..MAX_DROP) {
            if (canStandAt(world, x, y - drop, z)) return intArrayOf(x, y - drop, z)
        }
        return null
    }

    /** parity: `WalkNodeEvaluator`: something to stand on, and two blocks of room to stand in. */
    public fun canStandAt(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val ground = world.getBlockState(x, y - 1, z) ?: return false
        if (BlockShapes.isEmptyShape(ground)) return false
        return isOpen(world, x, y, z) && isOpen(world, x, y + 1, z)
    }

    private fun isOpen(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        val stateId = world.getBlockState(x, y, z) ?: return false
        return BlockShapes.isEmptyShape(stateId)
    }

    /** parity: the cost a mob pays for walking through something it would rather not. */
    private fun malus(
        world: WorldState,
        x: Int,
        y: Int,
        z: Int,
    ): Double {
        val path = BlockStateDefinitions.blockOf(world.getBlockState(x, y, z) ?: return 0.0)?.path ?: return 0.0
        return when (path) {
            "water" -> WATER_MALUS
            "lava", "fire", "soul_fire", "magma_block", "campfire", "soul_campfire", "sweet_berry_bush" ->
                DANGER_MALUS
            else -> 0.0
        }
    }

    private fun distance(
        fromX: Int,
        fromY: Int,
        fromZ: Int,
        toX: Int,
        toY: Int,
        toZ: Int,
    ): Double {
        val dx = (toX - fromX).toDouble()
        val dy = (toY - fromY).toDouble()
        val dz = (toZ - fromZ).toDouble()
        return sqrt(dx * dx + dy * dy + dz * dz)
    }

    /** parity: `Node.distanceTo`, the straight line the heuristic uses. */
    private fun heuristic(
        fromX: Int,
        fromY: Int,
        fromZ: Int,
        toX: Int,
        toY: Int,
        toZ: Int,
    ): Double = distance(fromX, fromY, fromZ, toX, toY, toZ) + abs(toY - fromY) * 0.0

    private fun reconstruct(
        cameFrom: Map<Long, Long>,
        goal: Long,
    ): List<IntArray> {
        val path = ArrayDeque<IntArray>()
        var current = goal
        while (true) {
            path.addFirst(intArrayOf(unpackX(current), unpackY(current), unpackZ(current)))
            current = cameFrom[current] ?: break
        }
        // The first entry is where it already is, which is not a step.
        if (path.isNotEmpty()) path.removeFirst()
        return path.toList()
    }

    private val HORIZONTAL = listOf(BlockFace.NORTH, BlockFace.SOUTH, BlockFace.WEST, BlockFace.EAST)

    private fun pack(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3ffffff shl 38) or (z.toLong() and 0x3ffffff shl 12) or (y.toLong() and 0xfff)

    private fun unpackX(packed: Long): Int = (packed shl 0 shr 38).toInt()

    private fun unpackY(packed: Long): Int = (packed shl 52 shr 52).toInt()

    private fun unpackZ(packed: Long): Int = (packed shl 26 shr 38).toInt()
}
