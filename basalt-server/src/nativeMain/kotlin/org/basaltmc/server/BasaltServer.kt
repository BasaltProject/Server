package org.basaltmc.server

import org.basaltmc.auth.OnlineMode
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.NativeThread
import org.basaltmc.io.ShutdownSignal
import org.basaltmc.io.TcpListener
import org.basaltmc.io.sleepMillis
import org.basaltmc.protocol.play.Gamemode
import org.basaltmc.protocol.status.ServerStatusPayload
import org.basaltmc.server.command.CommandDispatcher
import org.basaltmc.server.command.CommandEnvironment
import org.basaltmc.server.command.ConsoleReader
import org.basaltmc.server.command.registerVanillaCommands
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.config.GroupsConfig
import org.basaltmc.server.config.WorldsConfig
import org.basaltmc.server.player.Difficulty
import org.basaltmc.server.player.PlayerRegistry
import org.basaltmc.server.world.LoadedWorld
import org.basaltmc.server.world.WORLDS_ROOT
import org.basaltmc.server.world.WorldMetadata
import org.basaltmc.storage.PlayerStore
import org.basaltmc.text.Component
import org.basaltmc.text.LangTable
import org.basaltmc.text.TextColor
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
import kotlin.time.TimeSource

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
 * saved there ([RegionStore]), including chunks first seen while exploring, so revisiting old
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
    private val worldsConfig: WorldsConfig,
    private val groupsConfig: GroupsConfig,
    private val lang: LangTable = LangTable.EMPTY,
    /**
     * The world's own metadata, already loaded. `Main` needs the seed before this class exists (it
     * builds the terrain generator with it), so it loads it and hands it over rather than having it
     * read, and warned about, twice.
     */
    defaultWorldMetadata: WorldMetadata? = null,
    playersDatabasePath: String = PLAYERS_DATABASE,
    worldsRoot: String = WORLDS_ROOT,
) {
    private val logger = Logger("Server")
    private val playerRegistry = PlayerRegistry()
    private val accepting = AtomicInt(1)

    /** Guards against running the shutdown twice: `/stop` and a signal can arrive together. */
    private val shuttingDown = AtomicInt(0)
    private var listener: TcpListener? = null

    // Before `world`, which generates the startup chunks: declaration order is what makes the flag
    // visible to that generation at all, and GenerationMetrics is only exact single-threaded.
    init {
        GenerationMetrics.enabled = config.generationMetrics
    }

    /**
     * Player state, deliberately outside every world directory: `players.db` sits beside `worlds/`,
     * so a world can be deleted, copied or reset without touching a single row. That is the whole
     * reason it is not where vanilla puts it.
     */
    private val playerStore = PlayerStore.open(playersDatabasePath)

    /**
     * Every world `worlds.toml` declares, each with its own chunks, region files, generation
     * workers, settings and seed. Keyed by name, which is what a player's stored row refers to.
     */
    private val worlds: Map<String, LoadedWorld> =
        worldsConfig.worlds.associate { declaration ->
            val root = "$worldsRoot/${declaration.name}"
            val metadata =
                if (declaration.name == worldsConfig.default.name && defaultWorldMetadata != null) {
                    // Already loaded by `Main` to build the terrain generator; reusing it avoids
                    // reading the file, and warning about a seed mismatch, twice.
                    defaultWorldMetadata
                } else {
                    WorldMetadata.loadOrCreate(root, declaration.seed, WorldMetadata.DEFAULT_SPAWN, logger)
                }
            declaration.name to
                LoadedWorld(
                    declaration = declaration,
                    settings = worldsConfig.settingsFor(declaration),
                    metadata = metadata,
                    worldsRoot = worldsRoot,
                    logger = logger,
                    // One worker while measuring: GenerationMetrics counts without atomics on
                    // purpose, so its numbers are only exact when a single thread is generating.
                    generationWorkers = if (config.generationMetrics) 1 else 8,
                    overworldRouter = overworldRouter,
                    surfaceEnvironment = surfaceEnvironment,
                    villageGenerator = villageGenerator,
                )
        }

    private val defaultWorld: LoadedWorld = worlds.getValue(worldsConfig.default.name)

    public val worldSeed: Long get() = defaultWorld.metadata.seed

    init {
        for (world in worlds.values) {
            logger.info {
                "Loading world '${world.name}' (${world.declaration.type.name.lowercase()}, " +
                    "seed ${world.metadata.seed})."
            }
            world.warmSpawnArea(SPAWN_RADIUS_CHUNKS)
            world.saveDirty()
        }
        if (GenerationMetrics.enabled) {
            Logger("GenerationMetrics").info { "\n" + GenerationMetrics.report() }
            GenerationMetrics.reset()
        }
    }

    private val tickLoop =
        TickLoop(
            defaultWorld.settings.saveIntervalSeconds,
            onSaveInterval = ::persistAndUnload,
            onTick = ::shutdownIfSignalled,
        )

    private val commands =
        CommandDispatcher(
            CommandEnvironment(
                players = playerRegistry,
                groups = groupsConfig,
                worldSeed = worldSeed,
                maxPlayers = config.maxPlayers,
                stopServer = ::shutdown,
                lang = lang,
                defaultWorldName = defaultWorld.name,
                playerRepository = playerStore.players,
                worlds = worlds,
            ),
        ).also(::registerVanillaCommands)

    private val defaultGamemode =
        Gamemode.entries.firstOrNull { it.name.equals(config.gamemode, ignoreCase = true) } ?: Gamemode.SURVIVAL

    private val difficulty =
        Difficulty.entries.firstOrNull { it.name.equals(defaultWorld.settings.difficulty, ignoreCase = true) }
            ?: Difficulty.EASY

    /**
     * What the last collection actually reclaimed. The chunk counts above only account for what the
     * server means to hold; if memory climbs while they stay flat, the answer is here, in the gap
     * between what is live and what has been allocated and not yet collected.
     */
    @OptIn(kotlin.native.runtime.NativeRuntimeApi::class, ExperimentalStdlibApi::class)
    private fun heapReport(): String {
        val info = kotlin.native.runtime.GC.lastGCInfo ?: return "No collection has run yet."
        val before = info.memoryUsageBefore["heap"]?.totalObjectsSizeBytes ?: 0L
        val after = info.memoryUsageAfter["heap"]?.totalObjectsSizeBytes ?: 0L
        return "Heap: ${after / 1_048_576} MB live after the last collection " +
            "(${before / 1_048_576} MB before it, ${info.epoch} collections so far)."
    }

    /** Writes every online player as one transaction, on the same interval the worlds save on. */
    private fun persistPlayers() {
        val snapshots = playerRegistry.snapshot().mapNotNull { it.connection.storedSnapshot() }
        if (snapshots.isEmpty()) return
        runCatching { playerStore.players.saveAll(snapshots) }
            .onFailure { logger.error(it) { "Failed to save ${snapshots.size} player(s)" } }
    }

    /** One tick-loop hook for all of it: unloading has to save what it drops, so they belong together. */
    private fun persistAndUnload() {
        // Emitted before the work, not after: if this method ever stops completing, the last thing
        // in the log is where it got to, instead of nothing at all.
        logger.debug { "Sweep starting across ${worlds.size} world(s)." }
        for (world in worlds.values) world.saveDirty()
        persistPlayers()

        var loaded = 0
        var unloaded = 0
        for (world in worlds.values) {
            // Only the players actually in this world keep its chunks resident.
            val positions =
                playerRegistry
                    .snapshot()
                    .filter { it.worldName == world.name }
                    .map { (it.x.toInt() shr 4) to (it.z.toInt() shr 4) }
            unloaded += world.unloadUnreachable(positions, SPAWN_RADIUS_CHUNKS, RETAIN_MARGIN_CHUNKS)
            loaded += world.state.loadedChunkCount
        }

        // Reported every sweep, not only when something happens: when this server misbehaves it is
        // because one of these numbers climbs without settling, and knowing which one is the whole
        // diagnosis. At debug level because it is a diagnostic, not operator news.
        logger.debug { "Chunks: $loaded loaded across ${worlds.size} world(s), $unloaded unloaded. " + heapReport() }
    }

    /**
     * The bridge between a signal and an orderly stop: the handler only sets a flag (it may not do
     * anything else safely), and this is what notices, once per tick.
     */
    private fun shutdownIfSignalled() {
        if (!ShutdownSignal.isRequested) return
        logger.info { "Termination signal received, shutting down." }
        shutdown()
    }

    /**
     * Brings everything to a stop *before* saving, in that order and for that reason.
     *
     * Saving while the server is still live is not merely slow, it may never finish: connected
     * players keep moving, so sessions keep requesting chunks, the queues keep generating them, and
     * the tick loop keeps launching its own saves alongside this one. Each concurrent region write
     * materializes the whole region, so the memory to finish grows faster than the work does.
     *
     * So: disconnect everyone first (nothing new can be requested), stop the tick loop (no
     * competing save), let the queues drain (no chunk half-generated), and only then write, once.
     */
    private fun shutdown() {
        // Once only. `/stop` and a signal can arrive together, and running this twice would mean two
        // concurrent saves of the same regions, which is exactly what the ordering below avoids.
        if (!shuttingDown.compareAndSet(0, 1)) return

        // First of all, so nothing new can arrive while the rest is winding down. Without this a
        // client with auto-reconnect walks straight back in mid-save and starts requesting chunks
        // again, which is exactly the work shutting down is trying to finish.
        accepting.value = 0
        runCatching { listener?.close() }

        val players = playerRegistry.snapshot()
        if (players.isNotEmpty()) {
            // Saved here, synchronously, *before* disconnecting them. Disconnecting only closes the
            // socket; each session then saves itself on its own thread, which raced the database
            // being closed a few lines below. Whoever lost that race was not saved at all, so a
            // player who changed game mode and then stopped the server came back as they were.
            logger.info { "Saving ${players.size} player(s)." }
            persistPlayers()

            logger.info { "Disconnecting ${players.size} player(s)." }
            for (player in players) {
                runCatching { player.connection.disconnect(Component.text("Server closed", TextColor.RED)) }
                playerRegistry.remove(player.uuid)
            }
        }

        tickLoop.stop()
        for (world in worlds.values) world.stopGenerating(SHUTDOWN_DRAIN_TIMEOUT_MILLIS)

        logger.info { "Saving ${worlds.size} world(s)." }
        for (world in worlds.values) {
            world.saveDirty()
            world.flushRegions()
        }
        // The players were saved before being disconnected, above. This is the checkpoint that
        // leaves players.db as one self-contained file.
        runCatching { playerStore.close() }.onFailure { logger.error(it) { "Failed to close players.db" } }
        logger.info { "Stopping the server." }
        exitProcess(0)
    }

    @OptIn(ExperimentalTime::class)
    public fun start(startedAt: Instant = Clock.System.now()) {
        // The driver opens lazily, so without a query here a bad path or an unreadable database
        // would not be discovered until the first player joined, which is the worst possible moment
        // to find out. It also creates the file, so the layout is complete before anyone connects.
        logger.info { "Player database ready: ${playerStore.players.count()} player(s) known." }
        // Before the tick loop, which is what polls the flag it sets.
        ShutdownSignal.install()
        val listener = TcpListener.bind(config.port).also { this.listener = it }
        tickLoop.start()
        ConsoleReader.start(commands)
        val elapsedSeconds = (Clock.System.now() - startedAt).inWholeMilliseconds / 1000.0
        logger.info {
            "Done (${elapsedSeconds}s)! Listening on port ${config.port} " +
                "(${if (onlineMode != null) "online" else "offline"}-mode)."
        }
        while (accepting.value == 1) {
            val connection = runCatching { listener.accept() }.getOrNull() ?: break
            NativeThread.start {
                ClientSession(
                    connection,
                    ::currentStatus,
                    onlineMode = onlineMode,
                    playerRegistry = playerRegistry,
                    worlds = worlds,
                    defaultWorld = defaultWorld,
                    commands = commands,
                    defaultGamemode = defaultGamemode,
                    difficulty = difficulty,
                    playerStore = playerStore,
                ).run()
            }
        }

        awaitShutdown()
    }

    /**
     * Keeps `main` alive while a shutdown finishes on another thread.
     *
     * [shutdown] runs on whoever asked for it (the console thread, or the tick loop on a signal) and
     * ends by terminating the process. Its very first act is to close the listener, which unblocks
     * the `accept()` above and drops out of the loop, so without this wait `main` returns and takes
     * the process down *while the save is still running*. That is not hypothetical: it silently
     * truncated the shutdown after "Saving the world.", which is how `players.db` was being left
     * with all of its rows still in the write-ahead log.
     */
    private fun awaitShutdown() {
        if (shuttingDown.value == 0) return
        val deadline = TimeSource.Monotonic.markNow()
        while (deadline.elapsedNow().inWholeMilliseconds < SHUTDOWN_WAIT_TIMEOUT_MILLIS) {
            sleepMillis(20)
        }
        // Only reachable if the shutdown wedged: it normally ends the process itself long before.
        logger.error { "Shutdown did not finish within ${SHUTDOWN_WAIT_TIMEOUT_MILLIS}ms, exiting anyway." }
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

        // Beside worlds/, never inside one.
        const val PLAYERS_DATABASE = "players.db"

        // Beyond the view distance, so a chunk already requested or prefetched by a session is
        // still there when that session gets round to sending it.
        const val RETAIN_MARGIN_CHUNKS = 4

        // Long enough for an in-flight chunk to land, short enough that a wedged worker cannot
        // hold the shutdown open indefinitely.
        const val SHUTDOWN_DRAIN_TIMEOUT_MILLIS = 5_000L

        // Generous on purpose: saving a large world legitimately takes a while, and cutting a save
        // short is far worse than waiting for one.
        const val SHUTDOWN_WAIT_TIMEOUT_MILLIS = 120_000L
    }
}
