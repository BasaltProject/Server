package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.Chunk
import kotlin.math.abs

/**
 * The undecorated terrain around the chunk being decorated.
 *
 * A configured feature whose body reaches past the chunk edge reads through this instead of taking
 * the outside for air, which is what made a `LakeFeature` preflight reject every volume that was
 * not perfectly centred. World coordinates throughout: nothing here wraps or clips to the origin
 * chunk.
 */
public fun interface NeighbourTerrain {
    /** The pre-decoration block at a world position, or null when that position is out of reach. */
    public fun blockStateIdAt(
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Int?
}

/**
 * Generates a neighbour the first time a feature reads into it and keeps it while the generator is
 * still working nearby, so a chunk's terrain is paid for once and read by the decorations around
 * it rather than once per reader.
 *
 * [generateTerrain] must stop before decoration, or reading a neighbour would decorate it and
 * recurse. Deliberately not thread-safe: one cache belongs to one generation owner, which is what
 * the regional scheduler hands out.
 */
public class NeighbourTerrainCache(
    private val radius: Int = 1,
    private val capacity: Int = DEFAULT_CAPACITY,
    private val generateTerrain: (chunkX: Int, chunkZ: Int) -> Chunk,
) {
    // Insertion-ordered, so evicting the first entry evicts the least recently generated one.
    private val terrain = LinkedHashMap<Long, Chunk>()

    /** A view bounded to [radius] chunks around the one being decorated. */
    public fun around(
        chunkX: Int,
        chunkZ: Int,
    ): NeighbourTerrain =
        NeighbourTerrain { worldX, y, worldZ ->
            val neighbourX = worldX shr 4
            val neighbourZ = worldZ shr 4
            if (maxOf(abs(neighbourX - chunkX), abs(neighbourZ - chunkZ)) > radius) {
                null
            } else {
                blockAt(neighbourX, neighbourZ, worldX, y, worldZ)
            }
        }

    /** Forgets everything held, for an owner that is done generating. */
    public fun clear() {
        terrain.clear()
    }

    private fun blockAt(
        neighbourX: Int,
        neighbourZ: Int,
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Int? {
        val chunk = terrain.getOrPut(key(neighbourX, neighbourZ)) { generateTerrain(neighbourX, neighbourZ) }
        if (terrain.size > capacity) terrain.remove(terrain.keys.first())
        if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return null
        return chunk.getBlockStateId(worldX and 15, y, worldZ and 15)
    }

    private fun key(
        chunkX: Int,
        chunkZ: Int,
    ): Long = (chunkX.toLong() shl 32) or (chunkZ.toLong() and 0xFFFFFFFFL)

    private companion object {
        // A ring of two around the chunk in flight, so a row-by-row or spiral order keeps its
        // neighbours instead of regenerating them.
        const val DEFAULT_CAPACITY = 25
    }
}
