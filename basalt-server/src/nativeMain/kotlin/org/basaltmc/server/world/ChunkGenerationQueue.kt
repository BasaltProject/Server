package org.basaltmc.server.world

import org.basaltmc.io.NativeMutex
import org.basaltmc.io.NativeThread
import org.basaltmc.io.sleepMillis
import org.basaltmc.world.Chunk
import kotlin.concurrent.AtomicInt

/**
 * The one place chunks get generated off the thread that asked for them: a fixed pool of workers,
 * started once, pulling from a queue ordered by how close each chunk is to a player.
 *
 * This replaces spawning a fresh batch of OS threads per request. A player flying fast crosses a
 * chunk boundary several times a second, and each crossing used to mean creating (and detaching)
 * eight threads and then blocking that player's own connection thread until the slowest chunk in
 * the batch finished. Here the connection thread only ever enqueues; it picks up finished chunks
 * later, whenever it next has the socket in hand (see `ClientSession.flushReadyChunks`).
 *
 * [dimension] is carried on every request and the pool is per dimension, not global: it is the
 * seam a regional scheduler would shard on, so nothing here assumes one queue owns the world.
 *
 * Workers poll rather than wait on a condition variable, because `basalt-io` has no portable
 * condvar yet and adding one means three more `expect`/`actual` platform bindings. An idle worker
 * costs one map lookup per millisecond, which does not show up next to what generating a chunk
 * costs; the wait that actually mattered, the caller's, is gone entirely.
 */
public class ChunkGenerationQueue(
    private val world: WorldState,
    public val dimension: String,
    workerCount: Int = DEFAULT_WORKERS,
) {
    private val mutex = NativeMutex()
    private val queued = HashMap<Long, Int>()
    private val inProgress = HashSet<Long>()
    private val dirty = ArrayList<Chunk>()
    private val running = AtomicInt(1)

    init {
        repeat(workerCount) {
            NativeThread.start {
                while (running.value == 1) {
                    val next = takeNext()
                    if (next == null) {
                        sleepMillis(IDLE_POLL_MILLIS)
                        continue
                    }
                    generate(next)
                }
            }
        }
    }

    /** Enqueues [chunkX], [chunkZ] unless it is already loaded, queued or being generated. Lower [priority] runs first. */
    public fun request(
        chunkX: Int,
        chunkZ: Int,
        priority: Int,
    ) {
        val key = pack(chunkX, chunkZ)
        if (world.isLoaded(chunkX, chunkZ)) return
        mutex.withLock {
            if (key in inProgress) return@withLock
            val existing = queued[key]
            // A closer player asking for the same chunk should move it up, never back.
            if (existing == null || priority < existing) queued[key] = priority
        }
    }

    /**
     * Generates every one of [positions] and returns them in the same order, blocking until they
     * are all ready. For the paths that genuinely cannot proceed without the chunks in hand: the
     * startup batch, and a player's own initial view on login.
     */
    public fun awaitAll(positions: List<Pair<Int, Int>>): List<Chunk> {
        if (positions.isEmpty()) return emptyList()
        for ((index, position) in positions.withIndex()) request(position.first, position.second, index)
        while (positions.any { !world.isLoaded(it.first, it.second) }) sleepMillis(IDLE_POLL_MILLIS)
        return positions.map { world.getOrGenerateChunk(it.first, it.second) }
    }

    /**
     * Hands over every chunk generated since the last call, for the caller to persist. Chunks are
     * accumulated rather than written as they are produced because [WorldPersistence.saveChunks]
     * rewrites a whole region file per call, so saving one chunk at a time would recompress the
     * entire region up to 1024 times over.
     */
    public fun drainDirty(): List<Chunk> =
        mutex.withLock {
            if (dirty.isEmpty()) {
                emptyList()
            } else {
                val drained = dirty.toList()
                dirty.clear()
                drained
            }
        }

    public fun stop() {
        running.value = 0
    }

    private fun takeNext(): Long? =
        mutex.withLock {
            if (queued.isEmpty()) return@withLock null
            var bestKey = 0L
            var bestPriority = Int.MAX_VALUE
            var found = false
            for ((key, priority) in queued) {
                if (priority < bestPriority) {
                    bestPriority = priority
                    bestKey = key
                    found = true
                }
            }
            if (!found) return@withLock null
            queued.remove(bestKey)
            inProgress.add(bestKey)
            bestKey
        }

    private fun generate(key: Long) {
        val chunkX = unpackX(key)
        val chunkZ = unpackZ(key)
        try {
            val alreadyLoaded = world.isLoaded(chunkX, chunkZ)
            val chunk = world.getOrGenerateChunk(chunkX, chunkZ)
            if (!alreadyLoaded) mutex.withLock { dirty.add(chunk) }
        } finally {
            mutex.withLock { inProgress.remove(key) }
        }
    }

    public companion object {
        // Fixed rather than queried: there is still no portable core count in this project, and
        // oversubscribing a little is cheap now that these threads are created once, not per batch.
        public const val DEFAULT_WORKERS: Int = 8
        private const val IDLE_POLL_MILLIS = 1L

        public fun pack(
            chunkX: Int,
            chunkZ: Int,
        ): Long = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)

        private fun unpackX(key: Long): Int = key.toInt()

        private fun unpackZ(key: Long): Int = (key shr 32).toInt()
    }
}
