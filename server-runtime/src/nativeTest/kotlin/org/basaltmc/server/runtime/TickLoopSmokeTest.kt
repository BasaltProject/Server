package org.basaltmc.server.runtime

import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.concurrent.AtomicLong
import kotlin.test.Test
import kotlin.test.assertTrue
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/**
 * What the tick loop actually manages, measured rather than reasoned about: every one of these
 * reproduced a real report of a server sitting at 17-19 TPS with an idle tick thread.
 *
 * Rates are counted here rather than read off [TickLoop.tickRates], whose one-second buckets are
 * too coarse to say anything over a test's worth of seconds.
 */
@OptIn(ExperimentalTime::class)
class TickLoopSmokeTest {
    @Test
    fun idleLoopHoldsTwentyTicksPerSecond() {
        val rate = measure(seconds = 3)
        assertTrue(rate >= 19.5, "an idle loop managed $rate TPS")
    }

    @Test
    fun loopHoldsItsRateWhileEveryCoreIsBusy() {
        val burning = AtomicInt(1)
        repeat(BURN_THREADS) {
            NativeThread.start {
                var accumulator = 0L
                while (burning.value == 1) accumulator += accumulator xor 0x5DEECE66DL
            }
        }
        try {
            val rate = measure(seconds = 3)
            assertTrue(rate >= 19.0, "a loaded loop managed $rate TPS with $BURN_THREADS threads burning")
        } finally {
            burning.value = 0
        }
    }

    /**
     * The periodic sweep is seconds of region writing while a world is being explored, and it used
     * to run on this thread. Longer than the catch-up window, so the backlog was not caught up but
     * dropped: one 2.2s sweep cost forty-five ticks outright, which is the whole of a real server
     * reading 17-19 TPS with a tick thread whose median tick was seven microseconds.
     */
    @Test
    fun aSweepLongerThanTheCatchUpWindowDoesNotCostTicks() {
        val sweeps = AtomicInt(0)
        val rate =
            measure(seconds = 6, saveIntervalSeconds = 2) {
                sweeps.incrementAndGet()
                sleepMillis(SLOW_SWEEP_MILLIS)
            }
        assertTrue(rate >= 19.0, "a loop sweeping for ${SLOW_SWEEP_MILLIS}ms managed $rate TPS")
        assertTrue(sweeps.value >= 2, "the save interval stopped firing on time (${sweeps.value} sweeps in 6s)")
    }

    private fun measure(
        seconds: Int,
        saveIntervalSeconds: Int = 0,
        onSaveInterval: () -> Unit = {},
    ): Double {
        val ticks = AtomicLong(0)
        val loop =
            TickLoop(
                saveIntervalSeconds = saveIntervalSeconds,
                onSaveInterval = onSaveInterval,
                onTick = { ticks.addAndGet(1) },
            )
        val started = TimeSource.Monotonic.markNow()
        loop.start()
        sleepMillis(seconds * 1000L)
        val counted = ticks.value
        val elapsed = started.elapsedNow().inWholeMilliseconds
        loop.stop()
        return counted.toDouble() * 1000.0 / elapsed
    }

    private companion object {
        /** Comfortably more than any developer machine has cores, which is the point. */
        const val BURN_THREADS = 16

        /** What a real sweep measured at, mid-generation, with 937 chunks resident. */
        const val SLOW_SWEEP_MILLIS = 2200L
    }
}
