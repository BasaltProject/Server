package org.basaltmc.server.world

import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockStateDefinitions

/**
 * Writes into every leaf how far it stands from a trunk, once, when the chunk arrives.
 *
 * parity: `TreeFeature.updateLeaves`, which vanilla runs when it grows a tree: a breadth-first walk
 * out of the logs that stamps the `distance` property on each leaf it reaches. Everything after
 * that (leaf decay, `LeavesBlock.updateShape`) only ever reads the stored value, which is why a
 * random tick on leaves costs six reads in vanilla and not a search of the canopy.
 *
 * Trees are placed here inside a single chunk, so a chunk-wide pass gives the same answer the
 * per-tree one does, and it also fixes a chunk saved before this existed.
 */
public object LeafDistances {
    /** parity: `LeavesBlock.DECAY_DISTANCE`. */
    private const val DECAY_DISTANCE = 7

    private const val NOT_LEAF = -1

    /** Stamps the distance on the leaves of [chunk], and answers how many it had to correct. */
    public fun assign(chunk: Chunk): Int {
        val bounds = chunk.verticalBounds
        var lowest = Int.MAX_VALUE
        var highest = Int.MIN_VALUE
        for (index in 0 until chunk.sectionCount) {
            if (!chunk.section(index).mayContain { BlockConnections.isLeaves(it) }) continue
            if (index < lowest) lowest = index
            if (index > highest) highest = index
        }
        if (lowest > highest) return 0

        // One section either way: the trunk a canopy hangs from can sit below the lowest leaf.
        val fromSection = maxOf(0, lowest - 1)
        val toSection = minOf(chunk.sectionCount - 1, highest + 1)
        val baseY = bounds.minY + fromSection * 16
        val height = (toSection - fromSection + 1) * 16

        val cells = IntArray(height shl 8) { NOT_LEAF }
        var sources = IntArray(64)
        var sourceCount = 0
        for (y in 0 until height) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, baseY + y, z)
                    val cell = (y shl 8) or (z shl 4) or x
                    if (BlockConnections.preventsLeafDecay(stateId)) {
                        cells[cell] = 0
                        if (sourceCount == sources.size) sources = sources.copyOf(sources.size * 2)
                        sources[sourceCount++] = cell
                    } else if (BlockConnections.isLeaves(stateId)) {
                        cells[cell] = DECAY_DISTANCE
                    }
                }
            }
        }
        if (sourceCount == 0) return 0

        var current = sources.copyOf(sourceCount)
        for (distance in 1 until DECAY_DISTANCE) {
            var next = IntArray(current.size)
            var nextCount = 0
            for (cell in current) {
                val y = cell shr 8
                val z = (cell shr 4) and 15
                val x = cell and 15
                for (face in 0 until 6) {
                    val nx = x + FACE_X[face]
                    val ny = y + FACE_Y[face]
                    val nz = z + FACE_Z[face]
                    if (nx !in 0..15 || nz !in 0..15 || ny < 0 || ny >= height) continue
                    val neighbour = (ny shl 8) or (nz shl 4) or nx
                    if (cells[neighbour] != DECAY_DISTANCE) continue
                    cells[neighbour] = distance
                    if (nextCount == next.size) next = next.copyOf(maxOf(8, next.size * 2))
                    next[nextCount++] = neighbour
                }
            }
            if (nextCount == 0) break
            current = next.copyOf(nextCount)
        }

        // A canopy is a handful of distinct states, so resolving each one once covers the chunk.
        val statesByDistance = HashMap<Int, IntArray>()
        var corrected = 0
        for (cell in cells.indices) {
            val distance = cells[cell]
            if (distance <= 0) continue
            val y = baseY + (cell shr 8)
            val z = (cell shr 4) and 15
            val x = cell and 15
            val stateId = chunk.getBlockStateId(x, y, z)
            val wanted = statesByDistance.getOrPut(stateId) { withEveryDistance(stateId) }[distance]
            if (wanted == stateId) continue
            chunk.setBlockStateId(x, y, z, wanted)
            corrected++
        }
        return corrected
    }

    private fun withEveryDistance(stateId: Int): IntArray {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return IntArray(DECAY_DISTANCE + 1) { stateId }
        return IntArray(DECAY_DISTANCE + 1) { distance ->
            if (distance == 0) {
                stateId
            } else {
                BlockStateDefinitions.stateIdOf(block, mapOf("distance" to distance.toString()), stateId) ?: stateId
            }
        }
    }

    private val FACE_X = intArrayOf(0, 0, 0, 0, -1, 1)
    private val FACE_Y = intArrayOf(-1, 1, 0, 0, 0, 0)
    private val FACE_Z = intArrayOf(0, 0, -1, 1, 0, 0)
}
