package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.GenerationMetrics
import kotlin.concurrent.AtomicReference
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
 * recurse.
 *
 * Chunks generate concurrently, so one cache really is reached from several threads at once. It
 * holds an immutable map behind an [AtomicReference] and swaps it, the same trade-off
 * `VillageStructureGenerator` makes for its assemblies: a thread that loses the race discards its
 * own copy and takes the winner's, because generating the same neighbour twice is wasted work and
 * never a different result.
 */
public class NeighbourTerrainCache(
    private val radius: Int = 1,
    private val capacity: Int = DEFAULT_CAPACITY,
    private val generateTerrain: (chunkX: Int, chunkZ: Int) -> Chunk,
) {
    // Insertion-ordered, so dropping the first entry drops the least recently generated one.
    private val terrain = AtomicReference<Map<Long, Chunk>>(emptyMap())

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
        terrain.value = emptyMap()
    }

    private fun blockAt(
        neighbourX: Int,
        neighbourZ: Int,
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Int? {
        val key = key(neighbourX, neighbourZ)
        val chunk = terrain.value[key] ?: generateAndKeep(key, neighbourX, neighbourZ)
        if (y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) return null
        return chunk.getBlockStateId(worldX and 15, y, worldZ and 15)
    }

    private fun generateAndKeep(
        key: Long,
        neighbourX: Int,
        neighbourZ: Int,
    ): Chunk {
        val generated =
            GenerationMetrics.measure(GenerationMetrics.Phase.NEIGHBOURS) {
                generateTerrain(neighbourX, neighbourZ)
            }
        while (true) {
            val current = terrain.value
            current[key]?.let { return it }
            val trimmed = if (current.size >= capacity) current.entries.drop(1).associate { it.toPair() } else current
            if (terrain.compareAndSet(current, trimmed + (key to generated))) return generated
        }
    }

    private fun key(
        chunkX: Int,
        chunkZ: Int,
    ): Long = (chunkX.toLong() shl 32) or (chunkZ.toLong() and 0xFFFFFFFFL)

    private companion object {
        // Wide enough that a chunk's eight neighbours survive until they generate for real: at 25
        // the measured cost was 96 ms per chunk, about a neighbour and a half regenerated every
        // time, because concurrent generation interleaves several chunks' working sets.
        const val DEFAULT_CAPACITY = 128
    }
}
