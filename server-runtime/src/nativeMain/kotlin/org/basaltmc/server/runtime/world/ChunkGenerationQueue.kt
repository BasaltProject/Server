package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.io.NativeMutex
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt

/**
 * The one place chunks get generated off the thread that asked for them: a fixed pool of workers
 * pulling from a queue ordered by distance to a player. The connection thread only enqueues and
 * collects finished chunks later (see `ClientSession.flushReadyChunks`), instead of spawning eight
 * threads per chunk boundary crossing and blocking on the slowest of them.
 *
 * The pool is per [dimension], not global: that is the seam a regional scheduler would shard on.
 * Workers poll rather than wait on a condition variable, which `wire-io` cannot express
 * portably yet; an idle one costs a map lookup per millisecond.
 */
public class ChunkGenerationQueue(
    private val world: WorldState,
    public val dimension: String,
    workerCount: Int = DEFAULT_WORKERS,
) {
    private val mutex = NativeMutex()
    private val queued = HashMap<Long, Int>()
    private val inProgress = HashSet<Long>()
    private val running = AtomicInt(1)

    init {
        repeat(workerCount) {
            NativeThread.start {
                var idleSpins = 0
                while (running.value == 1) {
                    val next = takeNext()
                    if (next == null) {
                        // Back off while there is nothing to do. Eight workers each grabbing the
                        // lock every millisecond is thousands of acquisitions a second spent
                        // discovering the queue is empty, and it is contention everything else
                        // then has to win against.
                        idleSpins++
                        sleepMillis(
                            if (idleSpins >
                                IDLE_SPINS_BEFORE_BACKOFF
                            ) {
                                IDLE_BACKOFF_MILLIS
                            } else {
                                IDLE_POLL_MILLIS
                            },
                        )
                        continue
                    }
                    idleSpins = 0
                    generate(next)
                    // A deliberate breath between chunks. These eight threads otherwise hold the
                    // world's lock in a tight loop, and the single thread that saves, unloads and
                    // sends keep-alives never wins against them.
                    sleepMillis(BETWEEN_CHUNKS_MILLIS)
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
            if (existing != null) {
                if (priority < existing) queued[key] = priority
                return@withLock
            }
            if (queued.size >= CAPACITY) {
                // Requests arrive faster than chunks can be generated whenever a player moves at
                // any speed, so without a ceiling this grows without bound, and every extra entry
                // makes picking the next one slower, which makes it grow faster still. Past the
                // ceiling the farthest pending request is the one worth losing.
                val worstKey = queued.maxByOrNull { it.value }?.key ?: return@withLock
                if (queued.getValue(worstKey) <= priority) return@withLock
                queued.remove(worstKey)
            }
            queued[key] = priority
        }
    }

    /**
     * Forgets pending requests [shouldKeep] rejects. A player who has moved on leaves behind
     * requests for ground nobody will look at again; generating them costs the same as real work
     * and their results are retained until the next unload sweep notices.
     */
    public fun retainWhere(shouldKeep: (chunkX: Int, chunkZ: Int) -> Boolean): Int =
        mutex.withLock {
            val stale = queued.keys.filter { !shouldKeep(unpackX(it), unpackZ(it)) }
            for (key in stale) queued.remove(key)
            stale.size
        }

    /** Pending and in-flight counts, for the diagnostics that report where the server's work is going. */
    public fun pendingCount(): Int = mutex.withLock { queued.size + inProgress.size }

    /**
     * Generates every one of [positions] and returns them in the same order, blocking until they
     * are all ready. For the paths that genuinely cannot proceed without the chunks in hand: the
     * startup batch, and a player's own initial view on login.
     */
    public fun awaitAll(positions: List<Pair<Int, Int>>): List<Chunk> {
        if (positions.isEmpty()) return emptyList()
        for ((index, position) in positions.withIndex()) request(position.first, position.second, index)
        // One lock acquisition per poll, not one per position. Checking each of a view distance's
        // worth of positions individually, every millisecond, was hundreds of thousands of lock
        // acquisitions a second from a single thread, against which nothing else got a turn.
        //
        // Bounded, because waiting forever turns any lost worker into a server that never finishes
        // starting and never says why. Past the deadline the remaining chunks are generated here,
        // on this thread, which is slower but always finishes.
        var waited = 0L
        while (world.countMissing(positions) > 0 && waited < AWAIT_TIMEOUT_MILLIS) {
            sleepMillis(AWAIT_POLL_MILLIS)
            waited += AWAIT_POLL_MILLIS
        }
        return positions.map { world.getOrGenerateChunk(it.first, it.second) }
    }

    /**
     * Everything queued or in flight, as one snapshot. Callers ask once and then test against the
     * result, rather than calling in per chunk: the unload sweep does that test while holding the
     * world's own lock, and taking this one inside it, hundreds of times, starved the sweep out
     * entirely against eight workers looping on both locks.
     */
    public fun pendingKeys(): Set<Long> = mutex.withLock { HashSet<Long>(queued.keys).also { it.addAll(inProgress) } }

    /** Stops accepting work. Workers finish whatever chunk they are on, so pair this with [awaitIdle]. */
    public fun stop() {
        running.value = 0
        mutex.withLock { queued.clear() }
    }

    /**
     * Waits until nothing is being generated, up to [timeoutMillis]. Shutdown saves what is in
     * memory, so it has to happen after the last chunk lands rather than racing it.
     */
    public fun awaitIdle(timeoutMillis: Long) {
        var waited = 0L
        while (waited < timeoutMillis) {
            if (mutex.withLock { inProgress.isEmpty() }) return
            sleepMillis(IDLE_POLL_MILLIS)
            waited += IDLE_POLL_MILLIS
        }
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
        try {
            // A freshly generated chunk marks itself dirty as it is filled in, so persistence is
            // driven from the chunks themselves rather than from a list kept here.
            world.getOrGenerateChunk(unpackX(key), unpackZ(key))
        } finally {
            mutex.withLock { inProgress.remove(key) }
        }
    }

    public companion object {
        // Fixed rather than queried: there is still no portable core count in this project, and
        // oversubscribing a little is cheap now that these threads are created once, not per batch.
        public const val DEFAULT_WORKERS: Int = 8

        // Comfortably more than one player's view plus prefetch, and small enough that scanning it
        // to pick the next chunk stays cheap.
        private const val CAPACITY = 4096
        private const val IDLE_POLL_MILLIS = 1L
        private const val AWAIT_POLL_MILLIS = 10L
        private const val AWAIT_TIMEOUT_MILLIS = 60_000L
        private const val IDLE_SPINS_BEFORE_BACKOFF = 20
        private const val IDLE_BACKOFF_MILLIS = 20L
        private const val BETWEEN_CHUNKS_MILLIS = 1L

        public fun pack(
            chunkX: Int,
            chunkZ: Int,
        ): Long = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)

        private fun unpackX(key: Long): Int = key.toInt()

        private fun unpackZ(key: Long): Int = (key shr 32).toInt()
    }
}
