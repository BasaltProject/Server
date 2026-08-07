package org.basaltmc.server

import org.basaltmc.auth.OnlineMode
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpListener
import org.basaltmc.protocol.status.ServerStatusPayload
import org.basaltmc.server.command.ConsoleCommandRegistry
import org.basaltmc.server.command.ConsoleReader
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.player.PlayerRegistry
import org.basaltmc.server.world.WorldState
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor
import org.basaltmc.world.FlatWorldGenerator
import kotlin.system.exitProcess
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.Instant

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
    private val logger = Logger("Server")
    private val playerRegistry = PlayerRegistry()
    private val world =
        WorldState(
            (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).flatMap { x ->
                (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).map { z -> FlatWorldGenerator.generate(x, z) }
            },
        )
    private val tickLoop = TickLoop(playerRegistry)
    private val consoleCommands =
        ConsoleCommandRegistry().apply {
            register("stop") {
                logger.info { "Stopping the server." }
                // No world persistence yet (see `ChunkTracker`'s own KDoc): nothing to flush before
                // exiting. Revisit once chunks can actually be saved.
                exitProcess(0)
            }
            register("help") {
                logger.info { "Known commands: stop, help." }
            }
        }

    @OptIn(ExperimentalTime::class)
    public fun start(startedAt: Instant = Clock.System.now()) {
        val listener = TcpListener.bind(config.port)
        tickLoop.start()
        ConsoleReader.start(consoleCommands)
        val elapsedSeconds = (Clock.System.now() - startedAt).inWholeMilliseconds / 1000.0
        logger.info {
            "Done (${elapsedSeconds}s)! Listening on port ${config.port} " +
                "(${if (onlineMode != null) "online" else "offline"}-mode)."
        }
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
