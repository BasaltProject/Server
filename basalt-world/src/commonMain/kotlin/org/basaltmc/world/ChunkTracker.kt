package org.basaltmc.world

/** A chunk's column position, in chunk (not block) coordinates. */
public data class ChunkPos(
    val x: Int,
    val z: Int,
)

/**
 * Tracks which chunks a single player should have loaded, recomputing the load/unload delta as
 * their view center (their own chunk position, matching what `SetChunkCacheCenterPacket` tells the
 * client) or view distance changes. This only decides *which* chunks belong in the set: it doesn't
 * send packets, generate/load chunk data, or order the results (real vanilla sends newly-visible
 * chunks nearest-first in a spiral from the center, which this doesn't replicate, only the
 * resulting set, since ordering only affects how quickly nearby terrain appears, not correctness).
 *
 * The loaded set is a square (Chebyshev distance `<= radius`), matching `SetChunkCacheRadiusPacket`
 * elsewhere in this project (and how `BasaltServer`'s own fixed spawn area is already computed):
 * not vanilla's real circular-ish view area with its edge-of-render-distance ticket falloff, which
 * would need decompiling `ChunkMap`'s exact ticket/distance logic to replicate. Documented gap.
 */
public class ChunkTracker {
    private val loadedChunks = LinkedHashSet<ChunkPos>()

    public val loaded: Set<ChunkPos> get() = loadedChunks

    public data class Delta(
        val toLoad: List<ChunkPos>,
        val toUnload: List<ChunkPos>,
    )

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
