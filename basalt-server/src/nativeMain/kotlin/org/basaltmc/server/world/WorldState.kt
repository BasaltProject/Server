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
 * [getOrGenerateChunk] deliberately does **not** hold [mutex] while [loadOrGenerate] itself runs:
 * real terrain generation (density router, surface rules, decoration, village assembly) can take
 * anywhere from tens of milliseconds to over a second, and every other player's block reads/writes
 * and chunk loads share this same lock. Holding it for the whole generation call serializes the
 * entire world behind whichever single chunk is slowest to generate right now, which is exactly
 * what made the server appear to freeze while any player explored new terrain. Instead: check
 * under the lock (fast), generate outside it (slow, but no longer blocking anyone else), then
 * re-lock briefly to insert. Two threads racing to generate the very same never-before-seen chunk
 * both do the (deterministic, side-effect-free) generation work and the second one's result is
 * simply discarded in favor of whichever finished first: rare, and cheaper than making every
 * chunk load wait on every other one.
 *
 * [loadOrGenerate] is expected to consult the region files before generating anything: a chunk seen
 * once during exploration and saved must come back off disk on the next visit, player edits and
 * all, rather than being regenerated from the seed. [ChunkGenerationQueue] is what drives it, and
 * what collects the freshly generated chunks for persistence.
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

    public val loadedChunkCount: Int get() = mutex.withLock { chunksByPos.size }

    /** Only the chunks holding something the region files do not, which is all a save has to write. */
    public fun dirtyChunks(): List<Chunk> = mutex.withLock { chunksByPos.values.filter { it.isDirty } }

    /** Every loaded chunk [shouldKeep] rejects. Separate from [evictClean] so the caller can save them unlocked. */
    public fun evictionCandidates(shouldKeep: (chunkX: Int, chunkZ: Int) -> Boolean): List<Chunk> =
        mutex.withLock {
            chunksByPos.values.filter { !shouldKeep(it.chunkX, it.chunkZ) }
        }

    /**
     * Drops [chunks] from memory, skipping any that became dirty again since the caller saved them:
     * evicting a chunk whose changes are not on disk throws away whatever the player built in it,
     * and it will simply come up for eviction again on the next sweep.
     */
    public fun evictClean(chunks: List<Chunk>): Int =
        mutex.withLock {
            var removed = 0
            for (chunk in chunks) {
                if (chunk.isDirty) continue
                if (chunksByPos.remove(chunk.chunkX to chunk.chunkZ) != null) removed++
            }
            removed
        }

    /** Whether this chunk is already in memory, without generating it: what the queue and the send path poll. */
    public fun isLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean = mutex.withLock { (chunkX to chunkZ) in chunksByPos }

    /** The chunk if it is already in memory, or `null`. Never generates, so it is safe on a connection thread. */
    public fun chunkIfLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? = mutex.withLock { chunksByPos[chunkX to chunkZ] }

    /** Returns the chunk at this position, generating (but not sending or persisting) it first if unseen. */
    public fun getOrGenerateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val key = chunkX to chunkZ
        mutex.withLock { chunksByPos[key] }?.let { return it }
        val generated = loadOrGenerate(chunkX, chunkZ)
        return mutex.withLock { chunksByPos.getOrPut(key) { generated } }
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
