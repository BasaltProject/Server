package org.basaltmc.server.world

import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.concurrent.AtomicReference
import kotlin.test.Test
import kotlin.test.assertNull

/**
 * A chunk is sent from a connection thread while another thread is still writing blocks into it.
 * Widening the paletted storage swapped the array and the width separately, so a reader could pair
 * the new width with the old array and index off the end of it.
 */
class ChunkConcurrentReadSmokeTest {
    @Test
    fun readingASectionWhileItIsWrittenNeverIndexesOffTheEnd() {
        val section = ChunkSection()
        val running = AtomicInt(1)
        val failure = AtomicReference<Throwable?>(null)

        NativeThread.start {
            try {
                while (running.value == 1) section.blockStateIds()
            } catch (e: Throwable) {
                failure.value = e
            }
        }

        try {
            // Distinct states in ascending order, so the palette widens repeatedly under the reader.
            var stateId = 1
            repeat(WRITE_PASSES) {
                for (y in 0..15) {
                    for (z in 0..15) {
                        for (x in 0..15) {
                            section.setBlockStateId(x, y, z, stateId)
                            stateId++
                        }
                    }
                }
                stateId = 1
            }
        } finally {
            running.value = 0
        }

        sleepMillis(SETTLE_MILLIS)
        assertNull(failure.value, "a concurrent reader threw ${failure.value}")
    }

    private companion object {
        const val WRITE_PASSES = 4
        const val SETTLE_MILLIS = 50L
    }
}
