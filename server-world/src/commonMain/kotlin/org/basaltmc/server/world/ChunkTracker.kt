package org.basaltmc.server.world

/** A chunk's column position, in chunk (not block) coordinates. */
public data class ChunkPos(
    val x: Int,
    val z: Int,
)

/**
 * Which chunks one player should have loaded, as a load/unload delta whenever their view centre or
 * distance changes. It decides the set only: no packets, no chunk data, and not vanilla's
 * nearest-first spiral, which affects how fast terrain appears rather than correctness.
 *
 * The set is a square, Chebyshev distance `<= radius`, as `SetChunkCacheRadiusPacket` declares it.
 * Vanilla's circular area with its ticket falloff is a documented gap.
 */
public class ChunkTracker {
    private val loadedChunks = LinkedHashSet<ChunkPos>()

    public val loaded: Set<ChunkPos> get() = loadedChunks

    public data class Delta(
        val toLoad: List<ChunkPos>,
        val toUnload: List<ChunkPos>,
    )

    /**
     * Forgets everything, so the next [update] reports the whole view as newly loaded. A respawn
     * needs this: the client discards its own level when it respawns, so chunks it was previously
     * sent are gone on its side while this tracker still believed it had them.
     */
    public fun reset() {
        loadedChunks.clear()
    }

    public fun update(
        centerX: Int,
        centerZ: Int,
        radius: Int,
    ): Delta {
        require(radius >= 0) { "radius must be non-negative: $radius" }

        val desired = LinkedHashSet<ChunkPos>()
        for (x in centerX - radius..centerX + radius) {
            for (z in centerZ - radius..centerZ + radius) {
                desired += ChunkPos(x, z)
            }
        }

        val toLoad = desired.filter { it !in loadedChunks }
        val toUnload = loadedChunks.filter { it !in desired }

        loadedChunks.removeAll(toUnload.toSet())
        loadedChunks.addAll(toLoad)

        return Delta(toLoad, toUnload)
    }
}
