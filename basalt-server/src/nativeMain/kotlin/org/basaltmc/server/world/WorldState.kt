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

    // Keyed by packed coordinates, not by `Pair`. A Pair key allocates an object and boxes both
    // ints on *every* lookup, and these are the most called methods in the server: the login path
    // alone polls hundreds of positions per millisecond.
    private val chunksByPos =
        HashMap<Long, Chunk>().apply {
            for (chunk in initialChunks) this[key(chunk.chunkX, chunk.chunkZ)] = chunk
        }

    public val chunks: List<Chunk> get() = mutex.withLock { chunksByPos.values.toList() }

    public val loadedChunkCount: Int get() = mutex.withLock { chunksByPos.size }

    /**
     * Only the chunks holding something the region files do not, which is all a save has to write.
     *
     * Snapshots under the lock and filters outside it. Every generation worker needs this same lock
     * twice per chunk, so holding it across a scan of everything loaded, from a single thread
     * competing with eight, is how that thread ends up never being scheduled at all.
     */
    public fun dirtyChunks(): List<Chunk> = snapshot().filter { it.isDirty }

    /** Every loaded chunk [shouldKeep] rejects. Filtered outside the lock, for the reason [dirtyChunks] gives. */
    public fun evictionCandidates(shouldKeep: (chunkX: Int, chunkZ: Int) -> Boolean): List<Chunk> =
        snapshot().filter { !shouldKeep(it.chunkX, it.chunkZ) }

    private fun snapshot(): List<Chunk> = mutex.withLock { chunksByPos.values.toList() }

    /** How many of [positions] are still missing, counted in one pass under one lock acquisition. */
    public fun countMissing(positions: List<Pair<Int, Int>>): Int =
        mutex.withLock { positions.count { key(it.first, it.second) !in chunksByPos } }

    private companion object {
        fun key(
            chunkX: Int,
            chunkZ: Int,
        ): Long = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)
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
                if (chunksByPos.remove(key(chunk.chunkX, chunk.chunkZ)) != null) removed++
            }
            removed
        }

    /** Whether this chunk is already in memory, without generating it: what the queue and the send path poll. */
    public fun isLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean = mutex.withLock { key(chunkX, chunkZ) in chunksByPos }

    /** The chunk if it is already in memory, or `null`. Never generates, so it is safe on a connection thread. */
    public fun chunkIfLoaded(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk? = mutex.withLock { chunksByPos[key(chunkX, chunkZ)] }

    /** Returns the chunk at this position, generating (but not sending or persisting) it first if unseen. */
    public fun getOrGenerateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val key = key(chunkX, chunkZ)
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
            val chunk = chunksByPos[key(x shr 4, z shr 4)] ?: return@withLock null
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
            val chunk = chunksByPos[key(x shr 4, z shr 4)] ?: return@withLock false
            chunk.setBlockStateId(x and 15, y, z and 15, stateId)
            true
        }
}
