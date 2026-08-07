package org.basaltmc.server.world

import org.basaltmc.io.NativeMutex
import org.basaltmc.world.Chunk
import org.basaltmc.world.NoiseWorldGenerator

/**
 * Shared, mutable view over the currently loaded chunks, guarding both block mutations and the
 * chunk set itself with a single coarse-grained lock (matching
 * [org.basaltmc.server.player.PlayerRegistry]'s own pattern): every [org.basaltmc.server.ClientSession]
 * runs on its own thread, and two players breaking/placing blocks, or two players crossing into the
 * same never-before-seen chunk at once ([getOrGenerateChunk]), are both real data races without one.
 * Fine-grained (per-chunk/region) locking is future work once the scheduler ADR this project's own
 * roadmap tracks is settled; a single lock is the simplest correct starting point, matching that
 * ADR's own "global first" recommendation.
 *
 * Chunks generated on demand past the initial fixed startup set aren't persisted incrementally as
 * they're generated (only the whole set gets saved at `stop`, same as the initial set): a documented
 * gap, acceptable since [NoiseWorldGenerator] is deterministic per chunk position (round-tripping a
 * server restart mid-exploration regenerates the same terrain, just loses any player edits in chunks
 * that were never saved).
 */
public class WorldState(
    initialChunks: List<Chunk>,
    private val loadOrGenerate: (chunkX: Int, chunkZ: Int) -> Chunk = NoiseWorldGenerator::generate,
) {
    private val mutex = NativeMutex()
    private val chunksByPos =
        mutableMapOf<Pair<Int, Int>, Chunk>().apply {
            for (chunk in initialChunks) this[chunk.chunkX to chunk.chunkZ] = chunk
        }

    public val chunks: List<Chunk> get() = mutex.withLock { chunksByPos.values.toList() }

    /** Returns the chunk at this position, generating (but not sending or persisting) it first if unseen. */
    public fun getOrGenerateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        mutex.withLock {
            chunksByPos.getOrPut(chunkX to chunkZ) { loadOrGenerate(chunkX, chunkZ) }
        }

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
