package org.basaltmc.server

import org.basaltmc.assets.AssetCache
import org.basaltmc.assets.EulaFile
import org.basaltmc.assets.ServerJarAssets
import org.basaltmc.assets.VersionManifest
import org.basaltmc.auth.MojangSessionService
import org.basaltmc.auth.OnlineMode
import org.basaltmc.auth.SessionKeyPair
import org.basaltmc.common.logging.Logger
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.server.config.BasaltConfig
import org.basaltmc.server.world.RealNoiseParameters
import org.basaltmc.server.world.RealSurfaceEnvironment
import org.basaltmc.server.world.VillageDataLoader
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.terrain.NoiseRouterData
import org.basaltmc.world.terrain.VillageStructureGenerator
import kotlin.system.exitProcess
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

@OptIn(ExperimentalTime::class)
public fun main(args: Array<String>) {
    val startedAt = Clock.System.now()
    println("Basalt ${BuildInfo.VERSION} (protocol ${BuildInfo.PROTOCOL_VERSION})")

    // No readLine()/prompts, no TTY detection: identical behavior interactive, headless, or
    // containerized. --accept-eula is a deliberate operator act, not a fallback. (This is about
    // boot never blocking on input; the operator console `BasaltServer` starts once already
    // running is a different concern, see `ConsoleReader`'s own KDoc.)
    val logger = Logger("Eula")
    val eulaResult = EulaFile.check(acceptFlag = "--accept-eula" in args, logger = logger)
    if (eulaResult is EulaFile.Result.Rejected) {
        exitProcess(EulaFile.REJECTED_EXIT_CODE)
    }

    // Cached after the first run: `AssetCache.ensure` skips the download once the local copy's
    // SHA-1 already matches, so this only touches the network again if the cache is missing or stale.
    val assetsLogger = Logger("Assets")
    val versionMetadata = VersionManifest().resolve(BuildInfo.MINECRAFT_VERSION)
    val clientJarPath = AssetCache(logger = assetsLogger).ensure(versionMetadata, AssetCache.Artifact.CLIENT)
    assetsLogger.info { "Client assets ready at $clientJarPath." }
    val serverJarPath = AssetCache(logger = assetsLogger).ensure(versionMetadata, AssetCache.Artifact.SERVER)
    assetsLogger.info { "Server assets ready at $serverJarPath." }

    // Real Overworld density-function router (see NoiseRouterData/TerrainProvider), built once at
    // startup and reused for every generated chunk. WORLD_SEED is a fixed placeholder, not yet
    // read from config: documented gap, matches `NoiseWorldGenerator`'s own until real per-world
    // seed configuration exists.
    val serverJar = ServerJarAssets(serverJarPath, BuildInfo.MINECRAFT_VERSION)
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

    val configLogger = Logger("Config")
    val config = BasaltConfig.load(logger = configLogger)
    Logger.defaultMinLevel = config.logLevel

    val onlineMode = if (config.onlineMode) OnlineMode(SessionKeyPair.generate(), MojangSessionService()) else null
    BasaltServer(config, onlineMode, overworldRouter, surfaceEnvironment, villageGenerator).start(startedAt)
}

private const val WORLD_SEED = 0L
