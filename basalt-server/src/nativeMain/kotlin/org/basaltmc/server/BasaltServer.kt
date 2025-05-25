package org.basaltmc.server

import org.basaltmc.auth.OnlineMode
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.NativeThread
import org.basaltmc.io.TcpListener
import org.basaltmc.io.sleepMillis
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
import org.basaltmc.world.GenerationMetrics
import org.basaltmc.world.NoiseWorldGenerator
import org.basaltmc.world.terrain.OverworldNoiseRouter
import org.basaltmc.world.terrain.RealTerrainGenerator
import org.basaltmc.world.terrain.VillageStructureGenerator
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.concurrent.AtomicInt
import kotlin.system.exitProcess
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.Instant

/**
 * A thread-per-connection server, per `basalt-io`'s current scope (no event loop yet).
 * Handshake, Status, Login, Configuration and Play are implemented for a single world.
 * [SPAWN_RADIUS_CHUNKS] chunks generate eagerly at startup (a small warm cache so the server is
 * ready to listen fast, see [generateChunksInParallel]'s own KDoc); each joining player then gets
 * [BasaltConfig.viewDistance]'s full real view distance around their own spawn (generating
 * whatever that startup batch didn't already cover), and further chunks stream in as they move
 * ([org.basaltmc.server.ClientSession.streamChunksAround]). Each chunk is loaded from
 * [DIMENSION]'s region files under [BasaltConfig.worldName] if already
 * saved there ([WorldPersistence]); chunks missing from disk are generated fresh and saved
 * immediately (not only on a clean `stop`, so a crash or a `kill` right after first boot does not
 * throw away a freshly generated world that was never written to disk).
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

    private fun loadOrGenerateWorld(): WorldState {
        val positions =
            (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).flatMap { x ->
                (-SPAWN_RADIUS_CHUNKS..SPAWN_RADIUS_CHUNKS).map { z -> x to z }
            }
        val fromDisk =
            positions.associateWith { (x, z) ->
                WorldPersistence.loadChunk(config.worldName, DIMENSION, x, z)
            }
        val toGenerate = fromDisk.filterValues { it == null }.keys.toList()
        val freshlyGenerated = generateChunksInParallel(toGenerate)

        val chunks = positions.map { pos -> fromDisk.getValue(pos) ?: freshlyGenerated.getValue(pos) }
        if (freshlyGenerated.isNotEmpty()) {
            logger.info { "Generated ${freshlyGenerated.size} new chunk(s), saving them now." }
            saveWorld(freshlyGenerated.values.toList())
        }
        if (GenerationMetrics.enabled) {
            Logger("GenerationMetrics").info { "\n" + GenerationMetrics.report() }
            GenerationMetrics.reset()
        }
        return WorldState(chunks, loadOrGenerate = ::generateChunk)
    }

    /**
     * Splits [positions] round-robin across [STARTUP_GENERATION_WORKERS] worker threads: real
     * terrain generation (density router, surface rules, decoration, village assembly) measured
     * at tens of milliseconds to over a second per chunk, so generating the startup radius one
     * chunk at a time on the main thread was most of this server's own startup time. Each worker
     * writes only its own disjoint indices into [results] (no lock needed for the writes
     * themselves), and the caller busy-waits on [remaining] rather than joining, since
     * [NativeThread] is fire-and-forget by design (see its own KDoc). Safe to run before any
     * player connects: nothing else touches the world yet, so this doesn't race against the
     * concurrent-generation safety [WorldState]/`VillageStructureGenerator`/`VillageDataLoader`
     * added for in-game chunk streaming, it just exercises it a little earlier.
     */
    private fun generateChunksInParallel(positions: List<Pair<Int, Int>>): Map<Pair<Int, Int>, Chunk> {
        if (positions.isEmpty()) return emptyMap()
        val budget = if (GenerationMetrics.enabled) 1 else STARTUP_GENERATION_WORKERS
        val workerCount = minOf(budget, positions.size)
        val results = arrayOfNulls<Chunk>(positions.size)
        val remaining = AtomicInt(workerCount)
        for (worker in 0 until workerCount) {
            NativeThread.start {
                var i = worker
                while (i < positions.size) {
                    val (x, z) = positions[i]
                    results[i] = generateChunk(x, z)
                    i += workerCount
                }
                remaining.decrementAndGet()
            }
        }
        while (remaining.value > 0) sleepMillis(1L)
        return positions.indices.associate { positions[it] to results[it]!! }
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
                    configuredViewDistance = config.viewDistance,
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

        // A fixed worker count rather than a queried core count: this project has no portable
        // "how many cores" call yet (would need its own per-platform cinterop), and 8 is a
        // reasonable default that doesn't oversubscribe a small machine too badly while still
        // giving a real speedup over one chunk at a time.
        const val STARTUP_GENERATION_WORKERS = 8
    }
}
