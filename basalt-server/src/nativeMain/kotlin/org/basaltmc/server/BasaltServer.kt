package org.basaltmc.server

import org.basaltmc.auth.OnlineMode
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpListener
import org.basaltmc.protocol.status.ServerStatusPayload
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.player.PlayerRegistry
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor
import org.basaltmc.world.Chunk
import org.basaltmc.world.FlatWorldGenerator

/**
 * A thread-per-connection server, per `basalt-io`'s current scope (no event loop yet).
 * Handshake, Status, Login, Configuration and Play are implemented for a single fixed flat
 * world: [SPAWN_RADIUS_CHUNKS] chunks around spawn, generated once at startup, no real
 * view-distance-based loading yet.
 */
public class BasaltServer(
    private val config: BasaltConfig,
    private val onlineMode: OnlineMode? = null,
) {
    private val playerRegistry = PlayerRegistry()
    private val world: List<Chunk> =
        (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).flatMap { x ->
            (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).map { z -> FlatWorldGenerator.generate(x, z) }
        }
    private val tickLoop = TickLoop(playerRegistry)

    public fun start() {
        val listener = TcpListener.bind(config.port)
        tickLoop.start()
        println("Basalt listening on port ${config.port} (${if (onlineMode != null) "online" else "offline"}-mode)")
        while (true) {
            val connection = listener.accept()
            NativeThread.start {
                ClientSession(
                    connection,
                    ::currentStatus,
                    onlineMode = onlineMode,
                    playerRegistry = playerRegistry,
                    world = world,
                ).run()
            }
        }
    }

    private fun currentStatus(): ServerStatusPayload =
        ServerStatusPayload(
            description = Component.text(config.motd, TextColor.GRAY),
            players = ServerStatusPayload.Players(max = config.maxPlayers, online = 0),
            version = ServerStatusPayload.Version(BuildInfo.MINECRAFT_VERSION, BuildInfo.PROTOCOL_VERSION),
            favicon = Favicon.bytes,
        )

    private companion object {
        const val SPAWN_RADIUS_CHUNKS = 1
    }
}
