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
import org.basaltmc.server.world.ChunkGenerationQueue
import org.basaltmc.server.world.WorldPersistence
import org.basaltmc.server.world.WorldState
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor
import org.basaltmc.world.Chunk
import org.basaltmc.world.GenerationMetrics
import org.basaltmc.world.NoiseWorldGenerator
import org.basaltmc.world.terrain.OverworldNoiseRouter
import org.basaltmc.world.terrain.RealTerrainGenerator
import org.basaltmc.world.terrain.VillageStructureGenerator
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.system.exitProcess
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.Instant

/**
 * A thread-per-connection server, per `basalt-io`'s current scope (no event loop yet).
 * Handshake, Status, Login, Configuration and Play are implemented for a single world.
 * [SPAWN_RADIUS_CHUNKS] chunks generate eagerly at startup, a small warm cache so the server is
 * ready to listen fast; each joining player then gets [BasaltConfig.viewDistance]'s full real view
 * distance around their own spawn, and further chunks stream in as they move
 * ([org.basaltmc.server.ClientSession.streamChunksAround]). All of it goes through one
 * [ChunkGenerationQueue], the only thing in the server that generates terrain.
 *
 * Every chunk comes from [DIMENSION]'s region files under [BasaltConfig.worldName] if it was ever
 * saved there ([WorldPersistence]), including chunks first seen while exploring, so revisiting old
 * ground returns the same terrain with whatever was built on it rather than regenerating it.
 * Freshly generated chunks are written back on [TickLoop]'s save interval, not only on a clean
 * `stop`, so a crash costs at most that interval.
 *
 * [overworldRouter]/[surfaceEnvironment], when both given, generate new chunks with
 * [RealTerrainGenerator] (the real, jar-verified Overworld density-function router plus the real
 * `SurfaceRules` tree) instead of [NoiseWorldGenerator]'s stand-in approximation; [villageGenerator],
 * also optional, additionally places real Plains villages during that same generation call.
 * [Main.kt][org.basaltmc.server] always builds and passes real ones for actual gameplay; they stay
 * optional here (not required) so protocol/session-focused tests, whose concern is the network
 * layer, not terrain fidelity, can keep using the fast, network-free placeholder instead of
 * downloading a real `server.jar` and paying real generation cost for something they never
 * inspect. Terrain fidelity itself has its own dedicated, real-jar-verified tests
 * (`RealTerrainGeneratorTest`/`SurfaceBuilderTest` and friends), not these.
 */
public class BasaltServer(
    private val config: BasaltConfig,
    private val onlineMode: OnlineMode? = null,
    private val overworldRouter: OverworldNoiseRouter? = null,
    private val surfaceEnvironment: SurfaceRuleEnvironment? = null,
    private val villageGenerator: VillageStructureGenerator? = null,
) {
    private val logger = Logger("Server")
    private val playerRegistry = PlayerRegistry()

    // Before `world`, which generates the startup chunks: declaration order is what makes the flag
    // visible to that generation at all, and GenerationMetrics is only exact single-threaded.
    init {
        GenerationMetrics.enabled = config.generationMetrics
    }

    private val world = WorldState(emptyList(), loadOrGenerate = ::loadOrGenerateChunk)

    // One worker while measuring: GenerationMetrics counts without atomics on purpose, so its
    // numbers are only exact when a single thread is generating (see its own KDoc).
    private val generationQueue =
        ChunkGenerationQueue(world, DIMENSION, workerCount = if (config.generationMetrics) 1 else 8)
    private val spawn = warmStartupChunksAndComputeSpawn()
    private val tickLoop = TickLoop(playerRegistry, onSaveInterval = ::persistGeneratedChunks)
    private val consoleCommands =
        ConsoleCommandRegistry().apply {
            register("stop") {
                logger.info { "Saving the world." }
                generationQueue.stop()
                saveWorld(world.chunks)
                logger.info { "Stopping the server." }
                exitProcess(0)
            }
            register("help") {
                logger.info { "Known commands: stop, help." }
            }
        }

    private fun generateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val router = overworldRouter
        val environment = surfaceEnvironment
        return if (router != null && environment != null) {
            RealTerrainGenerator.generate(router, environment, chunkX, chunkZ, villageGenerator = villageGenerator)
        } else {
            NoiseWorldGenerator.generate(chunkX, chunkZ)
        }
    }

    /** A chunk off disk if it has ever been saved, so exploring back over old ground keeps player edits. */
    private fun loadOrGenerateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        WorldPersistence.loadChunk(config.worldName, DIMENSION, chunkX, chunkZ)
            ?: generateChunk(chunkX, chunkZ)

    /**
     * Generates the small warm cache around spawn, then reads the spawn height out of it: a player
     * who has not been here before lands on top of the terrain at the world's own (8, 8) column,
     * not a fixed Y that assumed a flat world.
     */
    private fun warmStartupChunksAndComputeSpawn(): Triple<Double, Double, Double> {
        val positions =
            (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).flatMap { x ->
                (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).map { z -> x to z }
            }
        generationQueue.awaitAll(positions)
        persistGeneratedChunks()
        if (GenerationMetrics.enabled) {
            Logger("GenerationMetrics").info { "\n" + GenerationMetrics.report() }
            GenerationMetrics.reset()
        }
        val spawnChunk = world.getOrGenerateChunk(0, 0)
        return Triple(8.5, spawnChunk.heightmap()[8 + 8 * 16].toDouble(), 8.5)
    }

    /**
     * Writes out whatever the queue has generated since the last call. Driven from [TickLoop] so a
     * crash or a `kill` mid-exploration loses at most one flush interval, instead of every chunk
     * generated since boot. Batched per call because [WorldPersistence.saveChunks] rewrites a whole
     * region file at a time, so this is one recompress per touched region, not one per chunk.
     */
    private fun persistGeneratedChunks() {
        val generated = generationQueue.drainDirty()
        if (generated.isEmpty()) return
        logger.info { "Saving ${generated.size} newly generated chunk(s)." }
        saveWorld(generated)
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
                    configuredViewDistance = config.viewDistance,
                    generationQueue = generationQueue,
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
