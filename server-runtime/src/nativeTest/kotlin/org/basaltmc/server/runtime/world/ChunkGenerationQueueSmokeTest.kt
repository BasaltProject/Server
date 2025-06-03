package org.basaltmc.server.runtime.world

import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.io.cpuCoreCount
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.test.Test
import kotlin.test.assertTrue

/** What the generation pool does when the machine is small and when a chunk will not generate. */
class ChunkGenerationQueueSmokeTest {
    @Test
    fun thePoolLeavesTheServerSomeCores() {
        val cores = cpuCoreCount()
        assertTrue(cores == null || cores > 0, "cpuCoreCount() reported $cores")
        val workers = ChunkGenerationQueue.DEFAULT_WORKERS
        assertTrue(workers >= 1, "the pool was sized to $workers workers")
        if (cores != null && cores > 2) {
            assertTrue(workers <= cores - 2, "$workers workers on a $cores-core machine leaves nothing for the server")
        }
    }

    /**
     * One chunk that will not generate used to end the worker that drew it, silently, so the pool
     * shrank a thread at a time and only the throughput ever showed it.
     */
    @Test
    fun aFailingChunkDoesNotCostAWorker() {
        val attempts = AtomicInt(0)
        val state =
            WorldState(emptyList()) { chunkX, chunkZ ->
                attempts.incrementAndGet()
                if (chunkX == POISONED_X) throw IndexOutOfBoundsException("poisoned chunk")
                Chunk(chunkX, chunkZ)
            }
        val queue = ChunkGenerationQueue(state, "minecraft:overworld", workerCount = 1)
        try {
            queue.request(POISONED_X, 0, priority = 0)
            waitUntil { attempts.value >= 1 }

            queue.request(1, 0, priority = 1)
            waitUntil { state.isLoaded(1, 0) }
            assertTrue(state.isLoaded(1, 0), "the only worker died on the failing chunk and never took the next one")
        } finally {
            queue.stop()
        }
    }

    private fun waitUntil(condition: () -> Boolean) {
        var waited = 0L
        while (!condition() && waited < TIMEOUT_MILLIS) {
            sleepMillis(POLL_MILLIS)
            waited += POLL_MILLIS
        }
    }

    private companion object {
        const val POISONED_X = -999
        const val POLL_MILLIS = 5L
        const val TIMEOUT_MILLIS = 5_000L
    }
}
