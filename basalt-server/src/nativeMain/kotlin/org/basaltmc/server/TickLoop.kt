package org.basaltmc.server

import org.basaltmc.io.NativeThread
import org.basaltmc.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.concurrent.AtomicReference
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/**
 * A 20 TPS heartbeat on its own thread. Global scheduling only; there is no
 * world/entity tick to run yet (no redstone, no mob AI).
 *
 * It deliberately touches **no sockets**. Sockets here are blocking, so writing to a client that
 * has stopped keeping up parks whichever thread is doing the writing until that one client drains.
 * Doing that from this shared thread stalled saving, chunk unloading and every other player at once
 * the moment a single client fell behind, which is exactly what happens to someone flying fast.
 * Anything that has to reach a client belongs on that client's own thread; see
 * [org.basaltmc.server.ClientSession].
 */
@OptIn(ExperimentalTime::class)
public class TickLoop(
    saveIntervalSeconds: Int = DEFAULT_SAVE_INTERVAL_SECONDS,
    private val onSaveInterval: () -> Unit = {},
    /**
     * Runs every tick. Deliberately not a general extension point: it exists so something checks
     * for a termination signal 20 times a second, which no other thread here can do (the accept
     * loop is blocked in `accept()` and connection threads only run while their client talks).
     */
    private val onTick: () -> Unit = {},
) {
    /** 0 seconds means no periodic save at all; chunks still go out when they unload or on stop. */
    private val saveIntervalTicks: Long = saveIntervalSeconds.toLong() * TICKS_PER_SECOND
    private val running = AtomicInt(0)
    private val rates = AtomicReference(TickRates.IDLE)

    /** How many ticks a second the loop is actually managing, over one, five and fifteen minutes. */
    public val tickRates: TickRates get() = rates.value

    public fun start() {
        running.value = 1
        NativeThread.start {
            val meter = TickMeter()
            var tick = 0L
            while (running.value == 1) {
                val started = TimeSource.Monotonic.markNow()
                tick++
                onTick()
                if (saveIntervalTicks > 0 && tick % saveIntervalTicks == 0L) onSaveInterval()

                rates.value = meter.tick()
                // Subtract the work, rather than always sleeping a whole tick. Without this the loop
                // ran at 50ms *plus* however long the tick took, so it could never reach 20 and a
                // measured rate would have been reporting the sleep instead of the server.
                val spent = started.elapsedNow().inWholeMilliseconds
                if (spent < TICK_MILLIS) sleepMillis(TICK_MILLIS - spent)
            }
        }
    }

    public fun stop() {
        running.value = 0
    }

    /** Ticks per second over the three windows, the way every server reports them. */
    public data class TickRates(
        val oneMinute: Double,
        val fiveMinutes: Double,
        val fifteenMinutes: Double,
    ) {
        public companion object {
            /** Before enough time has passed to have measured anything. */
            public val IDLE: TickRates =
                TickRates(TICKS_PER_SECOND.toDouble(), TICKS_PER_SECOND.toDouble(), TICKS_PER_SECOND.toDouble())
        }
    }

    /**
     * Counts ticks against the clock, in one-second buckets.
     *
     * Buckets rather than a running average of tick durations because they answer the question
     * actually being asked: not "how long did a tick take" but "how many happened", which is the
     * one that notices a loop that stalled completely.
     */
    private class TickMeter {
        private val start = TimeSource.Monotonic.markNow()
        private val seconds = ArrayDeque<Int>()
        private var currentSecond = 0L
        private var currentCount = 0

        fun tick(): TickRates {
            val second = start.elapsedNow().inWholeSeconds
            if (second != currentSecond) {
                // Fill in whole seconds where nothing ticked at all, or a stall would read as a gap
                // rather than as the zero it was.
                repeat((second - currentSecond).toInt().coerceAtMost(WINDOW_SECONDS)) {
                    seconds.addLast(if (it == 0) currentCount else 0)
                }
                while (seconds.size > WINDOW_SECONDS) seconds.removeFirst()
                currentSecond = second
                currentCount = 0
            }
            currentCount++
            return TickRates(rateOver(60), rateOver(300), rateOver(WINDOW_SECONDS))
        }

        /** Ticks per second over the last [window] seconds, or the nominal rate before that much has passed. */
        private fun rateOver(window: Int): Double {
            if (seconds.isEmpty()) return TICKS_PER_SECOND.toDouble()
            val counted = seconds.takeLast(window)
            val rate = counted.sum().toDouble() / counted.size
            return minOf(rate, TICKS_PER_SECOND.toDouble())
        }
    }

    public companion object {
        public const val TICK_MILLIS: Long = 50L
        public const val TICKS_PER_SECOND: Long = 20L

        /** Fifteen minutes, the longest window reported. */
        private const val WINDOW_SECONDS = 900

        // How much exploration a crash can cost, and an operator's to change (`save-interval-seconds`).
        // Linear compresses a region as one stream, so saving any chunk rewrites all 1024 of its
        // neighbours; that is affordable now only because a region streams straight to disk instead
        // of being built in memory first.
        public const val DEFAULT_SAVE_INTERVAL_SECONDS: Int = 30
    }
}
