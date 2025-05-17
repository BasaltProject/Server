package org.basaltmc.server

import org.basaltmc.io.NativeThread
import org.basaltmc.io.sleepMillis
import org.basaltmc.server.player.PlayerRegistry
import kotlin.concurrent.AtomicInt

/**
 * A 20 TPS heartbeat on its own thread. Global scheduling only: the only
 * thing it drives today is the keep-alive interval, since there is no world/entity tick to run
 * yet (no redstone, no mob AI). [playerRegistry] is passed in rather than looked up from a
 * singleton, so this stays viable if a regional scheduler replaces the global one later.
 */
public class TickLoop(
    private val playerRegistry: PlayerRegistry,
) {
    private val running = AtomicInt(0)

    public fun start() {
        running.value = 1
        NativeThread.start {
            var tick = 0L
            while (running.value == 1) {
                tick++
                if (tick % KEEP_ALIVE_INTERVAL_TICKS == 0L) sendKeepAlives()
                sleepMillis(TICK_MILLIS)
            }
        }
    }

    public fun stop() {
        running.value = 0
    }

    private fun sendKeepAlives() {
        for (player in playerRegistry.snapshot()) player.connection.sendKeepAlive()
    }

    private companion object {
        const val TICK_MILLIS = 50L
        const val KEEP_ALIVE_INTERVAL_TICKS = 300L // 15 real-time seconds, matching vanilla
    }
}
