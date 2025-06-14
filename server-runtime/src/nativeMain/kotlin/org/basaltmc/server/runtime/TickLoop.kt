package org.basaltmc.server.runtime

import org.basaltmc.wire.common.timings.TickDurations
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.sleepMillis
import kotlin.concurrent.AtomicInt
import kotlin.concurrent.AtomicLong
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
 * [org.basaltmc.server.runtime.ClientSession].
 */
@OptIn(ExperimentalTime::class)
public class TickLoop(
    saveIntervalSeconds: Int = DEFAULT_SAVE_INTERVAL_SECONDS,
    /** Every tick's duration, for the distribution `/timings` reports. */
    public val durations: TickDurations = TickDurations(),
    /**
     * The periodic sweep, on its own thread. On the tick thread it outlasted [CATCH_UP_LIMIT], so
     * its backlog was dropped rather than caught up and those ticks were simply lost.
     */
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

    // A flag, not a queue: a sweep that outlasts an interval owes one more sweep, not several.
    private val sweepRequested = AtomicInt(0)
    private val sweeping = AtomicInt(0)

    /** How many ticks a second the loop is actually managing, over one, five and fifteen minutes. */
    public val tickRates: TickRates get() = rates.value

    /**
     * `/tick` state: the rate asked for, whether the loop is frozen, and how many ticks a frozen
     * loop still owes. parity: `ServerTickRateManager`.
     *
     * Frozen means the loop keeps running at its own pace but does no work, so connections and
     * chunk streaming carry on: that is what vanilla's freeze does too, and it is why a frozen
     * server still answers a `/tick unfreeze`.
     */
    private val targetRate = AtomicReference(TICKS_PER_SECOND.toFloat())
    private val frozen = AtomicInt(0)
    private val stepsOwed = AtomicInt(0)
    private val ticksCounted = AtomicLong(0L)
    private val sprintTicksLeft = AtomicInt(0)

    public val tickRate: Float get() = targetRate.value
    public val isFrozen: Boolean get() = frozen.value == 1
    public val isSprinting: Boolean get() = sprintTicksLeft.value > 0

    public fun setTickRate(rate: Float) {
        targetRate.value = rate.coerceIn(MIN_TICK_RATE, MAX_TICK_RATE)
    }

    public fun setFrozen(value: Boolean) {
        frozen.value = if (value) 1 else 0
        if (!value) stepsOwed.value = 0
    }

    /** Ticks run since the loop started, which is what `/debug stop` reports over its window. */
    public val ticksRun: Long get() = ticksCounted.value

    /** Runs [ticks] ticks while frozen. Returns false when the loop is not frozen, as vanilla does. */
    public fun step(ticks: Int): Boolean {
        if (frozen.value != 1) return false
        stepsOwed.value = ticks
        return true
    }

    /** Runs [ticks] ticks as fast as they will go, ignoring the rate. Returns the ticks left to run. */
    public fun sprint(ticks: Int) {
        sprintTicksLeft.value = ticks
    }

    public fun stopSprinting(): Boolean {
        if (sprintTicksLeft.value <= 0) return false
        sprintTicksLeft.value = 0
        return true
    }

    public fun start() {
        running.value = 1
        if (saveIntervalTicks > 0) startSweeper()
        NativeThread.start {
            val meter = TickMeter()
            var tick = 0L
            val clock = TimeSource.Monotonic.markNow()
            var nextTickMillis = 0L
            while (running.value == 1) {
                val started = TimeSource.Monotonic.markNow()
                val sprinting = sprintTicksLeft.value > 0
                val stepping = stepsOwed.value > 0
                val skip = frozen.value == 1 && !stepping
                if (stepping) stepsOwed.value = stepsOwed.value - 1
                if (sprinting) sprintTicksLeft.value = sprintTicksLeft.value - 1
                tick++
                ticksCounted.value = tick
                if (!skip) onTick()
                if (saveIntervalTicks > 0 && tick % saveIntervalTicks == 0L) sweepRequested.value = 1

                // Recorded before the sleep, so it is the work that is measured and not the wait.
                durations.record(clock.elapsedNow().inWholeNanoseconds, started.elapsedNow().inWholeNanoseconds)
                rates.value = meter.tick()

                // Slept towards an absolute deadline rather than for "a tick minus the work".
                // sleepMillis only promises *at least* what it is asked for, so a computed delta
                // overshoots by a millisecond or two every tick and never gets it back: measured, that
                // drift alone held an idle server at 17 TPS. Counting deadlines from the start makes
                // an overshoot cost that tick and not the ones after it.
                // A sprint runs flat out, so the deadline follows the work rather than the clock.
                if (sprinting) {
                    nextTickMillis = clock.elapsedNow().inWholeMilliseconds
                    continue
                }
                nextTickMillis += (MILLIS_PER_SECOND / targetRate.value).toLong().coerceAtLeast(1L)
                val remaining = nextTickMillis - clock.elapsedNow().inWholeMilliseconds
                if (remaining > 0) {
                    sleepMillis(remaining)
                } else if (remaining < -TICK_MILLIS * CATCH_UP_LIMIT) {
                    // Too far behind to catch up by running faster; skip the backlog rather than
                    // spin through it, which is what turns a stall into a spiral.
                    nextTickMillis = clock.elapsedNow().inWholeMilliseconds
                }
            }
        }
    }

    /** Stops ticking and waits for a sweep in flight: the caller's next move is to save once itself. */
    public fun stop() {
        running.value = 0
        var waited = 0L
        while (sweeping.value == 1 && waited < STOP_DRAIN_MILLIS) {
            sleepMillis(SWEEP_POLL_MILLIS)
            waited += SWEEP_POLL_MILLIS
        }
    }

    private fun startSweeper() {
        NativeThread.start {
            while (running.value == 1) {
                if (!sweepRequested.compareAndSet(1, 0)) {
                    sleepMillis(SWEEP_POLL_MILLIS)
                    continue
                }
                sweeping.value = 1
                try {
                    onSaveInterval()
                } finally {
                    sweeping.value = 0
                }
            }
        }
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

        /** parity: the bounds `/tick rate` accepts. */
        public const val MIN_TICK_RATE: Float = 1f
        public const val MAX_TICK_RATE: Float = 10000f
        private const val MILLIS_PER_SECOND = 1000f
        public const val TICKS_PER_SECOND: Long = 20L

        /** How many ticks behind the loop tolerates before it stops trying to catch up. */
        private const val CATCH_UP_LIMIT = 20

        private const val SWEEP_POLL_MILLIS = 5L

        private const val STOP_DRAIN_MILLIS = 30_000L

        /** Fifteen minutes, the longest window reported. */
        private const val WINDOW_SECONDS = 900

        // How much exploration a crash can cost, and an operator's to change (`save-interval-seconds`).
        // Linear compresses a region as one stream, so saving any chunk rewrites all 1024 of its
        // neighbours; that is affordable now only because a region streams straight to disk instead
        // of being built in memory first.
        public const val DEFAULT_SAVE_INTERVAL_SECONDS: Int = 30
    }
}
