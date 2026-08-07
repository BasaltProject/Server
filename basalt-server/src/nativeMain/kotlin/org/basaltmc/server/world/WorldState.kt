package org.basaltmc.server.world

import org.basaltmc.io.NativeMutex
import org.basaltmc.world.Chunk

/**
 * Shared, mutable view over the currently loaded chunks, guarding block mutations with a single
 * coarse-grained lock (matching [org.basaltmc.server.player.PlayerRegistry]'s own pattern): every
 * [org.basaltmc.server.ClientSession] runs on its own thread, and two players breaking or placing
 * blocks at the same time is a real data race without one. Fine-grained (per-chunk/region) locking
 * is future work once the scheduler ADR this project's own roadmap tracks is settled; a single lock
 * is the simplest correct starting point, matching that ADR's own "global first" recommendation.
 *
 * The chunk *set* itself is still fixed at construction (no dynamic loading wired in yet, see
 * `ChunkTracker`'s own KDoc), so [chunks] is a plain unlocked read; only per-block state inside
 * those chunks needs the lock.
 */
public class WorldState(
    public val chunks: List<Chunk>,
) {
    private val mutex = NativeMutex()
    private val chunksByPos = chunks.associateBy { it.chunkX to it.chunkZ }

    public fun getBlockState(
        x: Int,
        y: Int,
        z: Int,
    ): Int? =
        mutex.withLock {
            val chunk = chunksByPos[(x shr 4) to (z shr 4)] ?: return@withLock null
            chunk.getBlockStateId(x and 15, y, z and 15)
        }

    /** Returns `true` if the position falls inside a loaded chunk (and the block was set), `false` otherwise. */
    public fun setBlockState(
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
    ): Boolean =
        mutex.withLock {
            val chunk = chunksByPos[(x shr 4) to (z shr 4)] ?: return@withLock false
            chunk.setBlockStateId(x and 15, y, z and 15, stateId)
            true
        }
}
