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
    private val onSaveInterval: () -> Unit = {},
) {
    private val running = AtomicInt(0)

    public fun start() {
        running.value = 1
        NativeThread.start {
            var tick = 0L
            while (running.value == 1) {
                tick++
                streamReadyChunks()
                if (tick % KEEP_ALIVE_INTERVAL_TICKS == 0L) sendKeepAlives()
                if (tick % SAVE_INTERVAL_TICKS == 0L) onSaveInterval()
                sleepMillis(TICK_MILLIS)
            }
        }
    }

    public fun stop() {
        running.value = 0
    }

    /** Pushes finished chunks out on the server's own clock, so delivery never depends on the client talking first. */
    private fun streamReadyChunks() {
        for (player in playerRegistry.snapshot()) player.connection.flushReadyChunks()
    }

    private fun sendKeepAlives() {
        for (player in playerRegistry.snapshot()) player.connection.sendKeepAlive()
    }

    private companion object {
        const val TICK_MILLIS = 50L
        const val KEEP_ALIVE_INTERVAL_TICKS = 300L // 15 real-time seconds, matching vanilla

        // How much exploration a crash can cost. Every 5 seconds, not every tick: each flush
        // rewrites one region file per region it touches.
        const val SAVE_INTERVAL_TICKS = 100L
    }
}
