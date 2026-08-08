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
    private val onSaveInterval: () -> Unit = {},
) {
    private val running = AtomicInt(0)

    public fun start() {
        running.value = 1
        NativeThread.start {
            var tick = 0L
            while (running.value == 1) {
                tick++
                if (tick % SAVE_INTERVAL_TICKS == 0L) onSaveInterval()
                sleepMillis(TICK_MILLIS)
            }
        }
    }

    public fun stop() {
        running.value = 0
    }

    private companion object {
        const val TICK_MILLIS = 50L
        // How much exploration a crash can cost. Every 5 seconds, not every tick: each flush
        // rewrites one region file per region it touches.
        const val SAVE_INTERVAL_TICKS = 100L
    }
}
