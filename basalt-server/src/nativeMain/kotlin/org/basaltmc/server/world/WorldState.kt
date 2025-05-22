package org.basaltmc.server.world

import org.basaltmc.io.NativeMutex
import org.basaltmc.io.NativeThread
import org.basaltmc.io.sleepMillis
import org.basaltmc.world.Chunk
import org.basaltmc.world.NoiseWorldGenerator
import kotlin.concurrent.AtomicInt

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
    ): Chunk {
        val key = chunkX to chunkZ
        mutex.withLock { chunksByPos[key] }?.let { return it }
        val generated = loadOrGenerate(chunkX, chunkZ)
        return mutex.withLock { chunksByPos.getOrPut(key) { generated } }
    }

    /**
     * Like [getOrGenerateChunk], but for a whole batch at once, generating any never-before-seen
     * positions across [GENERATION_WORKERS] worker threads instead of one at a time: what
     * [org.basaltmc.server.ClientSession.streamChunksAround] calls so a player moving fast (or
     * just connecting cold) doesn't serialize the whole batch of newly-visible chunks behind its
     * own single thread. Order of [positions] is preserved in the result.
     */
    public fun getOrGenerateChunks(positions: List<Pair<Int, Int>>): List<Chunk> {
        if (positions.isEmpty()) return emptyList()
        val results = arrayOfNulls<Chunk>(positions.size)
        val workerCount = minOf(GENERATION_WORKERS, positions.size)
        val remaining = AtomicInt(workerCount)
        for (worker in 0 until workerCount) {
            NativeThread.start {
                var i = worker
                while (i < positions.size) {
                    val (x, z) = positions[i]
                    results[i] = getOrGenerateChunk(x, z)
                    i += workerCount
                }
                remaining.decrementAndGet()
            }
        }
        while (remaining.value > 0) sleepMillis(1L)
        @Suppress("UNCHECKED_CAST")
        return results.asList() as List<Chunk>
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

    private companion object {
        const val GENERATION_WORKERS = 8
    }
}
