package org.basaltmc.server

import io.ktor.client.HttpClient
import org.basaltmc.assets.AssetCache
import org.basaltmc.assets.EulaFile
import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.assets.VersionManifest
import org.basaltmc.auth.MojangSessionService
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionKeyPair
import org.basaltmc.common.logging.LogOutput
import org.basaltmc.common.logging.Logger
import org.basaltmc.io.LogFile
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.config.ColorMode
import org.basaltmc.server.world.RealNoiseParameters
import org.basaltmc.server.world.RealSurfaceEnvironment
import org.basaltmc.server.world.VillageDataLoader
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.terrain.NoiseRouterData
import org.basaltmc.world.terrain.VillageStructureGenerator
import platform.posix.STDOUT_FILENO
import platform.posix.isatty
import kotlin.system.exitProcess
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

@OptIn(ExperimentalTime::class)
public fun main(args: Array<String>) {
    val startedAt = Clock.System.now()
    println(BANNER)
    println(versionLine())

    // No readLine()/prompts, no TTY detection: identical behavior interactive, headless, or
    // containerized. --accept-eula is a deliberate operator act, not a fallback. (This is about
    // boot never blocking on input; the operator console `BasaltServer` starts once already
    // running is a different concern, see `ConsoleReader`'s own KDoc.)
    val logger = Logger("Eula")
    val eulaResult = EulaFile.check(acceptFlag = "--accept-eula" in args, logger = logger)
    if (eulaResult is EulaFile.Result.Rejected) {
        exitProcess(EulaFile.REJECTED_EXIT_CODE)
    }

    // Escape codes are a terminal affordance; piped or redirected output gets none. Applied before
    // the config is read so that loading it is itself coloured, then re-applied once the operator's
    // own choice is known.
    val attachedToTerminal = isatty(STDOUT_FILENO) == 1
    LogOutput.useColor = attachedToTerminal

    // Before anything else that logs: configuration decides where log lines go, so loading it later
    // would mean the whole asset and world-generation startup never reached the log file.
    val config = BasaltConfig.load(logger = Logger("Config"))
    Logger.defaultMinLevel = config.logLevel
    LogOutput.useColor =
        when (config.logColor) {
            ColorMode.ALWAYS -> true
            ColorMode.NEVER -> false
            ColorMode.AUTO -> attachedToTerminal
        }
    if (config.logToFile) {
        LogFile.install(LOG_ROOT, keepDays = config.logKeepDays, header = versionLine())
    }

    // Downloading is the fallback, not the path: once a version's data is unpacked the lambda below
    // is never invoked, so a warm install resolves no metadata, contacts no network and reads no
    // archive. Only the server data is fetched; the client download holds assets nothing reads yet
    // (see `LangAssets`, still unused), and fetching it bought a log line.
    val assetsLogger = Logger("Assets")
    val dataRoot = "./.basalt/assets/${BuildInfo.MINECRAFT_VERSION}"
    val serverJar =
        ServerJarAssets(BuildInfo.MINECRAFT_VERSION, dataRoot, assetsLogger) {
            HttpClient().use { httpClient ->
                val versionMetadata = VersionManifest(httpClient).resolveCached(BuildInfo.MINECRAFT_VERSION)
                AssetCache(httpClient, logger = assetsLogger).ensure(versionMetadata, AssetCache.Artifact.SERVER)
            }
        }
    assetsLogger.info { "${BuildInfo.MINECRAFT_VERSION} data ready at $dataRoot." }

    // Real Overworld density-function router (see NoiseRouterData/TerrainProvider), built once at
    // startup and reused for every generated chunk. WORLD_SEED is a fixed placeholder, not yet
    // read from config: documented gap, matches `NoiseWorldGenerator`'s own until real per-world
    // seed configuration exists.
    val noiseRandom = XoroshiroRandom(WORLD_SEED).forkPositional()
    val realNoises = RealNoiseParameters(serverJar, noiseRandom)
    val base3d = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)
    val overworldRouter = NoiseRouterData.overworld(realNoises::get, base3d)
    val surfaceEnvironment = RealSurfaceEnvironment(serverJar, noiseRandom, overworldRouter)

    // Real villages.json placement values (spacing 34, separation 8, salt 10387312) and each
    // variant's own start_pool/size (all five share size 6 and max_distance_from_center 80): see
    // VillageStructureGenerator's own KDoc for the documented gaps in what actually gets placed.
    val villageLoader = VillageDataLoader(serverJar)
    val villageGrid = StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312)
    val villageGenerator =
        VillageStructureGenerator(
            overworldRouter,
            villageGrid,
            biomeAt = surfaceEnvironment::biomeAt,
            maxDepth = 6,
            maxDistanceFromCenter = 80,
            poolLookup = villageLoader.poolLookup,
            templateLoader = villageLoader.templateLoader,
        )

    val onlineMode = if (config.onlineMode) OnlineMode(SessionKeyPair.generate(), MojangSessionService()) else null
    BasaltServer(config, onlineMode, overworldRouter, surfaceEnvironment, villageGenerator).start(startedAt)
}

private fun versionLine(): String =
    "Basalt Server ${BuildInfo.VERSION} (protocol ${BuildInfo.PROTOCOL_VERSION})"

private const val WORLD_SEED = 0L
private const val LOG_ROOT = "./.basalt/logs"

// Raw string, and deliberately not indented: the art is full of backslashes, and every leading
// space in it is significant. One blank line above and one below come from the newlines this
// string opens and closes with, plus println's own.
private const val BANNER =
    """
  ____                  _ _      _____
 |  _ \                | | |    / ____|
 | |_) | __ _ ___  __ _| | |_  | (___   ___ _ ____   _____ _ __
 |  _ < / _` / __|/ _` | | __|  \___ \ / _ \ '__\ \ / / _ \ '__|
 | |_) | (_| \__ \ (_| | | |_   ____) |  __/ |   \ V /  __/ |
 |____/ \__,_|___/\__,_|_|\__| |_____/ \___|_|    \_/ \___|_|
"""
