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
import org.basaltmc.server.world.WorldPersistence
import org.basaltmc.server.world.WorldState
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor
import org.basaltmc.world.Chunk
import org.basaltmc.world.NoiseWorldGenerator
import kotlin.system.exitProcess
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.Instant

/**
 * A thread-per-connection server, per `basalt-io`'s current scope (no event loop yet).
 * Handshake, Status, Login, Configuration and Play are implemented for a single fixed flat
 * world: [SPAWN_RADIUS_CHUNKS] chunks around spawn, no real view-distance-based loading yet.
 * Each chunk is loaded from [DIMENSION]'s region files under [BasaltConfig.worldName] if already
 * saved there ([WorldPersistence]); chunks missing from disk are generated fresh and saved
 * immediately (not only on a clean `stop`, so a crash or a `kill` right after first boot does not
 * throw away a freshly generated world that was never written to disk).
 */
public class BasaltServer(
    private val config: BasaltConfig,
    private val onlineMode: OnlineMode? = null,
) {
    private val logger = Logger("Server")
    private val playerRegistry = PlayerRegistry()
    private val world = loadOrGenerateWorld()
    private val spawn = computeSpawn()
    private val tickLoop = TickLoop(playerRegistry)
    private val consoleCommands =
        ConsoleCommandRegistry().apply {
            register("stop") {
                logger.info { "Saving the world." }
                saveWorld(world.chunks)
                logger.info { "Stopping the server." }
                exitProcess(0)
            }
            register("help") {
                logger.info { "Known commands: stop, help." }
            }
        }

    private fun loadOrGenerateWorld(): WorldState {
        val generated = mutableListOf<Chunk>()
        val chunks =
            (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).flatMap { x ->
                (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).map { z ->
                    WorldPersistence.loadChunk(config.worldName, DIMENSION, x, z)
                        ?: NoiseWorldGenerator.generate(x, z).also { generated += it }
                }
            }
        if (generated.isNotEmpty()) {
            logger.info { "Generated ${generated.size} new chunk(s), saving them now." }
            saveWorld(generated)
        }
        return WorldState(chunks)
    }

    /** Where a player who has not been here before spawns: on top of the terrain at the world's own (8, 8) column, not a fixed Y that assumed a flat world. */
    private fun computeSpawn(): Triple<Double, Double, Double> {
        val spawnChunk = world.chunks.first { it.chunkX == 0 && it.chunkZ == 0 }
        val surfaceY = spawnChunk.heightmap()[8 + 8 * 16]
        return Triple(8.5, surfaceY.toDouble(), 8.5)
    }

    @OptIn(ExperimentalTime::class)
    private fun saveWorld(chunks: List<Chunk>) {
        WorldPersistence.saveChunks(
            config.worldName,
            DIMENSION,
            chunks,
            Clock.System
                .now()
                .epochSeconds
                .toInt(),
        )
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
                    spawn = spawn,
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
        // 1 declared a view/simulation distance of 1 chunk to the client, below what a real
        // client expects and handles well (vanilla's own slider does not even go that low): it
        // looked like "only one chunk renders" even though every chunk actually had real terrain
        // (confirmed directly by reading the saved region files back). 3 keeps startup bounded
        // while staying in the range a real client behaves normally at.
        const val SPAWN_RADIUS_CHUNKS = 3
        const val DIMENSION = "overworld"
    }
}
