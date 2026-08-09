package org.basaltmc.server

import org.basaltmc.io.NativeThread
import org.basaltmc.io.sleepMillis
import kotlin.concurrent.AtomicInt

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

    public fun start() {
        running.value = 1
        NativeThread.start {
            var tick = 0L
            while (running.value == 1) {
                tick++
                onTick()
                if (saveIntervalTicks > 0 && tick % saveIntervalTicks == 0L) onSaveInterval()
                sleepMillis(TICK_MILLIS)
            }
        }
    }

    public fun stop() {
        running.value = 0
    }

    public companion object {
        public const val TICK_MILLIS: Long = 50L
        public const val TICKS_PER_SECOND: Long = 20L

        // How much exploration a crash can cost, and an operator's to change (`save-interval-seconds`).
        // Linear compresses a region as one stream, so saving any chunk rewrites all 1024 of its
        // neighbours; that is affordable now only because a region streams straight to disk instead
        // of being built in memory first.
        public const val DEFAULT_SAVE_INTERVAL_SECONDS: Int = 30
    }
}
