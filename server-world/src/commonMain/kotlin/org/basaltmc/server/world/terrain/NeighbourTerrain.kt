package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.GenerationMetrics
import kotlin.concurrent.AtomicReference
import kotlin.math.abs

/**
 * The undecorated terrain around the chunk being decorated, so a feature reaching past the edge does
 * not take the outside for air. World coordinates throughout.
 */
public interface NeighbourTerrain {
    /** The pre-decoration block at a world position, or null when that position is out of reach. */
    public fun blockStateIdAt(
        worldX: Int,
        y: Int,
        worldZ: Int,
    ): Int?

    /**
     * The `OCEAN_FLOOR` height of a column outside the chunk, or null when out of reach. // parity: an ore
     * blob asks for every column its box covers, and a stricter preflight also skips the draws it would
     * have made.
     */
    public fun oceanFloorHeightAt(
        worldX: Int,
        worldZ: Int,
    ): Int?
}

/**
 * Generates a neighbour the first time a feature reads into it, then keeps it. [generateTerrain] must
 * stop before decoration or reading a neighbour recurses. Reached from several threads at once, so it
 * swaps an immutable map behind an [AtomicReference]: the loser of a race takes the winner's copy.
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
        object : NeighbourTerrain {
            override fun blockStateIdAt(
                worldX: Int,
                y: Int,
                worldZ: Int,
            ): Int? = inReach(worldX, worldZ)?.let { (nx, nz) -> blockAt(nx, nz, worldX, y, worldZ) }

            override fun oceanFloorHeightAt(
                worldX: Int,
                worldZ: Int,
            ): Int? =
                inReach(worldX, worldZ)?.let { (nx, nz) ->
                    oceanFloorAt(nx, nz, worldX, worldZ)
                }

            private fun inReach(
                worldX: Int,
                worldZ: Int,
            ): Pair<Int, Int>? {
                val neighbourX = worldX shr 4
                val neighbourZ = worldZ shr 4
                if (maxOf(abs(neighbourX - chunkX), abs(neighbourZ - chunkZ)) > radius) return null
                return neighbourX to neighbourZ
            }
        }

    /** Forgets everything held, for an owner that is done generating. */
    public fun clear() {
        terrain.value = emptyMap()
    }

    private fun oceanFloorAt(
        neighbourX: Int,
        neighbourZ: Int,
        worldX: Int,
        worldZ: Int,
    ): Int {
        val key = key(neighbourX, neighbourZ)
        val chunk = terrain.value[key] ?: generateAndKeep(key, neighbourX, neighbourZ)
        // The frozen one: whoever asks a neighbour for its floor is standing in for a `_WG` read,
        // and the live map already has that neighbour's own structures piled on it.
        return chunk.worldgenHeightAt(worldX and 15, worldZ and 15, Chunk.HeightmapType.OCEAN_FLOOR)
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
