package org.basaltmc.server.runtime

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.CommandEnvironment
import org.basaltmc.server.runtime.command.ConsoleCommandSource
import org.basaltmc.server.runtime.command.commands.AdvancementCommand
import org.basaltmc.server.runtime.command.commands.AttributeCommand
import org.basaltmc.server.runtime.command.commands.BanCommand
import org.basaltmc.server.runtime.command.commands.BanIpCommand
import org.basaltmc.server.runtime.command.commands.BanListCommand
import org.basaltmc.server.runtime.command.commands.BossBarCommand
import org.basaltmc.server.runtime.command.commands.BossBarRegistry
import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.command.commands.DamageCommand
import org.basaltmc.server.runtime.command.commands.DataAccess
import org.basaltmc.server.runtime.command.commands.DataCommand
import org.basaltmc.server.runtime.command.commands.DataPackCommand
import org.basaltmc.server.runtime.command.commands.DebugCommand
import org.basaltmc.server.runtime.command.commands.DialogCommand
import org.basaltmc.server.runtime.command.commands.EffectCommand
import org.basaltmc.server.runtime.command.commands.EnchantCommand
import org.basaltmc.server.runtime.command.commands.ExecuteCommand
import org.basaltmc.server.runtime.command.commands.FetchProfileCommand
import org.basaltmc.server.runtime.command.commands.FillBiomeCommand
import org.basaltmc.server.runtime.command.commands.ForceLoadCommand
import org.basaltmc.server.runtime.command.commands.FunctionCommand
import org.basaltmc.server.runtime.command.commands.FunctionRunner
import org.basaltmc.server.runtime.command.commands.FunctionSchedule
import org.basaltmc.server.runtime.command.commands.ItemCommand
import org.basaltmc.server.runtime.command.commands.JfrCommand
import org.basaltmc.server.runtime.command.commands.LocateCommand
import org.basaltmc.server.runtime.command.commands.LootCommand
import org.basaltmc.server.runtime.command.commands.PardonCommand
import org.basaltmc.server.runtime.command.commands.ParticleCommand
import org.basaltmc.server.runtime.command.commands.PerfCommand
import org.basaltmc.server.runtime.command.commands.PlaceCommand
import org.basaltmc.server.runtime.command.commands.PublishCommand
import org.basaltmc.server.runtime.command.commands.RandomCommand
import org.basaltmc.server.runtime.command.commands.RecipeCommand
import org.basaltmc.server.runtime.command.commands.ReloadCommand
import org.basaltmc.server.runtime.command.commands.RestartCommand
import org.basaltmc.server.runtime.command.commands.ReturnCommand
import org.basaltmc.server.runtime.command.commands.RideCommand
import org.basaltmc.server.runtime.command.commands.RotateCommand
import org.basaltmc.server.runtime.command.commands.SaveAllCommand
import org.basaltmc.server.runtime.command.commands.SaveControl
import org.basaltmc.server.runtime.command.commands.SaveOffCommand
import org.basaltmc.server.runtime.command.commands.SaveOnCommand
import org.basaltmc.server.runtime.command.commands.ScheduleCommand
import org.basaltmc.server.runtime.command.commands.ScoreboardCommand
import org.basaltmc.server.runtime.command.commands.SetIdleTimeoutCommand
import org.basaltmc.server.runtime.command.commands.SpawnPointCommand
import org.basaltmc.server.runtime.command.commands.SpectateCommand
import org.basaltmc.server.runtime.command.commands.SpreadPlayersCommand
import org.basaltmc.server.runtime.command.commands.StopwatchCommand
import org.basaltmc.server.runtime.command.commands.Stopwatches
import org.basaltmc.server.runtime.command.commands.SummonCommand
import org.basaltmc.server.runtime.command.commands.SwingCommand
import org.basaltmc.server.runtime.command.commands.TeamCommand
import org.basaltmc.server.runtime.command.commands.TeamMsgCommand
import org.basaltmc.server.runtime.command.commands.TestCommand
import org.basaltmc.server.runtime.command.commands.TickCommand
import org.basaltmc.server.runtime.command.commands.TimingsCommand
import org.basaltmc.server.runtime.command.commands.TransferCommand
import org.basaltmc.server.runtime.command.commands.TriggerCommand
import org.basaltmc.server.runtime.command.commands.UnpublishCommand
import org.basaltmc.server.runtime.command.commands.WaypointCommand
import org.basaltmc.server.runtime.command.commands.WhitelistCommand
import org.basaltmc.server.runtime.command.commands.WorldBorderCommand
import org.basaltmc.server.runtime.command.registerVanillaCommands
import org.basaltmc.server.runtime.config.BasaltConfig
import org.basaltmc.server.runtime.config.LiveConfigs
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.config.WorldsConfig
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.server.runtime.placeholder.AnimationPlaceholders
import org.basaltmc.server.runtime.placeholder.PlayerPlaceholders
import org.basaltmc.server.runtime.placeholder.ServerFacts
import org.basaltmc.server.runtime.placeholder.ServerPlaceholders
import org.basaltmc.server.runtime.player.Difficulty
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.server.runtime.timings.ReportSources
import org.basaltmc.server.runtime.timings.TimingsReport
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.AdvancementTriggers
import org.basaltmc.server.runtime.world.Beds
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.EndPodium
import org.basaltmc.server.runtime.world.EndPortals
import org.basaltmc.server.runtime.world.FluidFlow
import org.basaltmc.server.runtime.world.FluidKind
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.server.runtime.world.NetherPortals
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.RealAdvancements
import org.basaltmc.server.runtime.world.RealBlockTags
import org.basaltmc.server.runtime.world.RealDamageTypeTags
import org.basaltmc.server.runtime.world.RealDialogs
import org.basaltmc.server.runtime.world.RealEnchantments
import org.basaltmc.server.runtime.world.RealEntityTypeTags
import org.basaltmc.server.runtime.world.RealFluidTags
import org.basaltmc.server.runtime.world.RealItemTags
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.RealRecipes
import org.basaltmc.server.runtime.world.RealTestInstances
import org.basaltmc.server.runtime.world.RealTimelineTags
import org.basaltmc.server.runtime.world.RealTimelines
import org.basaltmc.server.runtime.world.RecipeBook
import org.basaltmc.server.runtime.world.StructureTemplates
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.VanillaWorldGeneratorFactory
import org.basaltmc.server.runtime.world.WORLDS_ROOT
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.world.GenerationMetrics
import org.basaltmc.server.world.portal.PortalAxis
import org.basaltmc.server.world.portal.PortalShape
import org.basaltmc.server.world.terrain.OverworldNoiseRouter
import org.basaltmc.server.world.terrain.VillageStructureGenerator
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.auth.OnlineMode
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.common.timings.TickDurations
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.common.timings.time
import org.basaltmc.wire.io.NativeThread
import org.basaltmc.wire.io.ShutdownSignal
import org.basaltmc.wire.io.TcpListener
import org.basaltmc.wire.io.sleepMillis
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.LangTable
import org.basaltmc.wire.text.TextColor
import org.basaltmc.wire.text.resolvePlainText
import kotlin.concurrent.AtomicInt
import kotlin.math.floor
import kotlin.random.Random
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.Instant
import kotlin.time.TimeSource
import kotlin.uuid.Uuid

/**
 * A thread-per-connection server, per `wire-io`'s current scope (no event loop yet), covering
 * Handshake, Status, Login, Configuration and Play for a single world.
 *
 * [SPAWN_RADIUS_CHUNKS] chunks generate eagerly so it starts listening fast and the rest stream as
 * players move, through the one [ChunkGenerationQueue]. New chunks are written on [TickLoop]'s
 * save interval, so a crash costs that interval rather than everything since boot.
 *
 * [worldGeneratorFactory] and the [overworldRouter]/[surfaceEnvironment] injection stay optional,
 * so protocol tests need no `server.jar`.
 */
public class BasaltServer(
    private val config: BasaltConfig,
    private val onlineMode: OnlineMode? = null,
    private val overworldRouter: OverworldNoiseRouter? = null,
    private val surfaceEnvironment: SurfaceRuleEnvironment? = null,
    private val villageGenerator: VillageStructureGenerator? = null,
    private val worldGeneratorFactory: VanillaWorldGeneratorFactory? = null,
    /** Where block-break drops are read from (see [RealLootTables]); `null` in tests that never break a block in survival. */
    private val serverJar: ServerJarAssets? = null,
    private val worldsConfig: WorldsConfig,
    /**
     * Everything `/reload` can swap under a running server, read through this rather than copied:
     * see [LiveConfigs] for where the line between reloadable and not is drawn and why.
     */
    public val live: LiveConfigs,
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
    private val cpuMeter = CpuMeter()
    private val lootTables = RealLootTables(serverJar)
    private val recipes = RealRecipes(serverJar)
    private val blockTags = RealBlockTags(serverJar)
    private val itemTags = RealItemTags(serverJar)
    private val fluidTags = RealFluidTags(serverJar)
    private val timelineTags = RealTimelineTags(serverJar)

    init {
        // Loud on purpose: a client whose overworld timeline tag is empty renders a sky that never
        // moves, and nothing else in the log would say why.
        val overworldTimelines = timelineTags.membersOf(Identifier.vanilla("in_overworld"))
        if (overworldTimelines.isEmpty()) {
            logger.warn {
                "No timelines are tagged for the overworld, so the sky will not move. " +
                    "The server jar's data is missing or could not be read."
            }
        } else {
            logger.debug { "Overworld timelines: ${overworldTimelines.joinToString()}." }
        }
    }

    private val damageTypeTags = RealDamageTypeTags(serverJar)
    private val entityTags = RealEntityTypeTags(serverJar)
    private val advancements = RealAdvancements(serverJar)
    private val enchantments = RealEnchantments(serverJar)
    private val dialogs = RealDialogs(serverJar)
    private val timelines = RealTimelines(serverJar)
    private val testInstances = RealTestInstances(serverJar)

    /** Structure files, loaded on demand: what `/place template` puts down. */
    private val structureTemplates = StructureTemplates(serverJar)

    /**
     * What `/timings` measures. Off until asked for: a clock read per section per call is small but
     * not free, and a server nobody is investigating should not pay it.
     */
    public val timings: Timings = Timings()
    private val tickDurations = TickDurations()
    private val serverClock = TimeSource.Monotonic.markNow()
    private val accepting = AtomicInt(1)

    /** Guards against running the shutdown twice: `/stop` and a signal can arrive together. */
    private val shuttingDown = AtomicInt(0)

    /** Set once the shutdown has finished its last write, so the caller knows it is safe to proceed. */
    private val shutdownComplete = AtomicInt(0)

    /** Whether this instance stopped in order to be replaced by a fresh one in the same process. */
    private val restarting = AtomicInt(0)

    public val shouldRestart: Boolean get() = restarting.value == 1
    private var listener: TcpListener? = null

    // Before `world`, which generates the startup chunks: declaration order is what makes the flag
    // visible to that generation at all, and GenerationMetrics is only exact single-threaded.
    init {
        GenerationMetrics.enabled = config.generationMetrics
        // A percentage needs two readings; this is the first, so nobody asking later gets nothing.
        cpuMeter.prime()
    }

    /**
     * Player state, deliberately outside every world directory: `players.db` sits beside `worlds/`,
     * so a world can be deleted, copied or reset without touching a single row. That is the whole
     * reason it is not where vanilla puts it.
     */
    private val playerStore = PlayerStore.open(playersDatabasePath)

    /**
     * What each player has been granted or taught. Declared after [playerStore] because it writes
     * through to it: a property initializer that reads one declared below it reads an uninitialized
     * field. See [PlayerProgress].
     */
    private val playerProgress = PlayerProgress(playerStore.progress)

    /**
     * What turns playing into an advancement.
     *
     * The announcement goes to everyone, as `PlayerList.broadcastSystemMessage` does, and only when
     * the default world's `show_advancement_messages` allows it: game rules are per world here, and
     * an advancement belongs to the player rather than to the world they happen to stand in.
     */
    private val advancementTriggers =
        AdvancementTriggers(
            advancements = advancements,
            progress = playerProgress,
            itemTagMembers = { tag -> itemTags.membersOf(tag.toString()) },
            announce = { _, message ->
                if (defaultWorld.gameRules[GameRuleRegistry.SHOW_ADVANCEMENT_MESSAGES]) {
                    for (player in playerRegistry.snapshot()) player.sendSystemMessage(message)
                    // Resolved, not serialized: a client translates the key itself, a log cannot,
                    // and this one printed `chat.type.advancement.task` at an operator.
                    logger.info { message.resolvePlainText(lang) }
                }
            },
            awardExperience = { name, amount ->
                playerRegistry.snapshot().firstOrNull { it.name == name }?.let { player ->
                    player.experience.givePoints(amount)
                    player.connection.syncExperience()
                }
            },
            awardRecipe = { name, recipe ->
                playerRegistry.snapshot().firstOrNull { it.name == name }?.let {
                    RecipeBook.add(it, recipes, listOf(recipe))
                }
            },
        )

    /**
     * Every world's `level.nbt`, read before any world is built.
     *
     * Overworlds first, and that order is the point: a Nether or an End that declares no seed of
     * its own takes the seed of the overworld it belongs to, which is what makes one set of
     * dimensions one world. parity: a vanilla save has a single seed for its three dimensions.
     */
    private val worldMetadata: Map<String, WorldMetadata> =
        buildMap {
            for (declaration in worldsConfig.worlds.sortedBy { it.dimension != WorldDimension.OVERWORLD }) {
                val metadata =
                    if (declaration.name == worldsConfig.default.name && defaultWorldMetadata != null) {
                        // Already loaded by `Main` to build the terrain generator; reusing it avoids
                        // reading the file, and warning about a seed mismatch, twice.
                        defaultWorldMetadata
                    } else {
                        val inherited = get(worldsConfig.overworldOf(declaration).name)?.seed
                        WorldMetadata.loadOrCreate(
                            "$worldsRoot/${declaration.name}",
                            declaration.seed ?: inherited,
                            WorldMetadata.DEFAULT_SPAWN,
                            logger,
                        )
                    }
                put(declaration.name, metadata)
            }
        }

    /**
     * Every world `worlds.toml` declares, each with its own chunks, region files, generation
     * workers, settings and seed. Keyed by name, which is what a player's stored row refers to.
     */
    private val worlds: Map<String, LoadedWorld> =
        worldsConfig.worlds.associate { declaration ->
            val metadata = worldMetadata.getValue(declaration.name)
            val generators =
                if (declaration.type == WorldType.NORMAL || declaration.type == WorldType.AMPLIFIED) {
                    worldGeneratorFactory?.create(declaration, metadata)
                } else {
                    null
                }
            declaration.name to
                LoadedWorld(
                    declaration = declaration,
                    settings = worldsConfig.settingsFor(declaration),
                    metadata = metadata,
                    worldsRoot = worldsRoot,
                    logger = logger,
                    timings = timings,
                    // One worker while measuring: GenerationMetrics counts without atomics on
                    // purpose, so its numbers are only exact when a single thread is generating.
                    generationWorkers = if (config.generationMetrics) 1 else ChunkGenerationQueue.DEFAULT_WORKERS,
                    overworldRouter = overworldRouter,
                    surfaceEnvironment = surfaceEnvironment,
                    // The one generation itself uses, so `/place structure` and `/locate structure`
                    // act on the same villages the world already has.
                    villageGenerator = generators?.villages ?: villageGenerator,
                    normalGenerator = generators?.generate,
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
            // parity: `EnderDragonFight` builds the podium the first time the End is loaded.
            if (world.declaration.dimension == WorldDimension.END) {
                EndPodium.ensure(world, active = world.settings.openEndExitPortal)?.let { origin ->
                    logger.info {
                        val state = if (world.settings.openEndExitPortal) "open" else "closed"
                        "Built the End's exit podium at ${origin[0]}, ${origin[1]}, ${origin[2]} ($state)."
                    }
                }
            }
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
            durations = tickDurations,
            onSaveInterval = { timings.of("tick.save").time { if (saveControl.enabled) persistAndUnload() } },
            onTick = ::tickWorlds,
        )

    /**
     * Every placeholder this server answers, in one registry shared by everything that renders
     * operator-written text. The server list is the first user; chat formats and the tab list will
     * be the next, and a plugin registering its own identifier is the point of it being a registry
     * rather than a `when` inside the server list.
     */
    public val placeholders: PlaceholderRegistry =
        PlaceholderRegistry().apply {
            register("basalt_player", PlayerPlaceholders({ live.current.groups }) { worlds })
            register(
                AnimationPlaceholders.IDENTIFIER,
                AnimationPlaceholders({ live.current.animations }) { placeholders },
            )
            register(
                "basalt_server",
                ServerPlaceholders(
                    ServerFacts(
                        versionName = BuildInfo.MINECRAFT_VERSION,
                        protocolVersion = BuildInfo.PROTOCOL_VERSION,
                        startedAt = Clock.System.now(),
                        maxPlayers = { config.maxPlayers },
                        onlineByWorld = {
                            playerRegistry.snapshot().groupingBy { it.worldName }.eachCount()
                        },
                        uniqueJoins = { playerStore.players.count() },
                        loadedChunks = { worlds.values.sumOf { it.state.loadedChunkCount } },
                        liveHeapBytes = { liveHeapBytes() },
                        tickRates = {
                            tickLoop.tickRates.let { Triple(it.oneMinute, it.fiveMinutes, it.fifteenMinutes) }
                        },
                        cpuPercent = cpuMeter::percent,
                    ),
                ),
            )
        }

    /** Objectives and scores, shared by `/scoreboard` and `/trigger`. */
    public val scoreboard: Scoreboard = Scoreboard()

    /** The one report `/timings`, `/debug` and `/perf` all write, so they cannot disagree. */
    private val timingsReport: TimingsReport by lazy {
        TimingsReport(
            timings,
            tickDurations,
            ReportSources(
                versionName = BuildInfo.MINECRAFT_VERSION,
                protocolVersion = BuildInfo.PROTOCOL_VERSION,
                uptimeSeconds = { serverClock.elapsedNow().inWholeSeconds },
                onlinePlayers = { playerRegistry.snapshot().size },
                maxPlayers = { config.maxPlayers },
                loadedChunks = { worlds.values.sumOf { world -> world.state.loadedChunkCount } },
                regionCacheBytes = { worlds.values.sumOf { it.regions.cachedRegionBytes } },
                chunkBytes = { worlds.values.sumOf { it.state.chunkMemory().bytes } },
                chunkBlockEntities = { worlds.values.sumOf { it.state.chunkMemory().blockEntities } },
                chunkScheduledTicks = { worlds.values.sumOf { it.state.chunkMemory().scheduledTicks } },
                chunkGeneratedEntities = { worlds.values.sumOf { it.state.chunkMemory().generatedEntities } },
                worldgenCaches = {
                    val stats = worldGeneratorFactory?.cacheStats()
                    if (stats == null) {
                        emptyList()
                    } else {
                        listOf(
                            stats.templates.toLong(),
                            stats.templateBlocks,
                            stats.assemblies.toLong(),
                            stats.assemblyPieces,
                        )
                    }
                },
                worldCount = { worlds.size },
                cpuPercent = cpuMeter::percent,
                liveHeapBytes = { collectedLiveHeapBytes() },
                residentBytes = {
                    org.basaltmc.wire.io
                        .processResidentBytes()
                },
            ),
        )
    }

    /** Functions from `datapacks/`, reloaded by `/reload` along with the configuration files. */
    public val functions: FunctionLibrary = FunctionLibrary().also { it.reload() }

    private val functionRunner = FunctionRunner(functions)
    private val functionSchedule = FunctionSchedule()

    /** Who a scheduled function runs as: the console, which is what vanilla uses for the same job. */
    private val consoleSource = ConsoleCommandSource(logger, lang)

    /** Named stopwatches `/stopwatch` keeps, in wall-clock time as vanilla counts them. */
    private val stopwatches = Stopwatches { serverClock.elapsedNow().inWholeMilliseconds }

    /** Named NBT compounds `/data ... storage` keeps, read back from disk at startup. */
    public val commandStorage: CommandStorage = CommandStorage().also { it.load() }

    /** Teams, shared by `/team` and `/teammsg`. */
    public val teams: Teams = Teams()

    /** The `/bossbar` bars this server knows, shown to whoever joins and is named by one. */
    public val bossBars: BossBarRegistry = BossBarRegistry()

    /** `/save-off` stops the periodic sweep; `/save-all` runs one on demand. */
    private val saveControl: SaveControl = SaveControl(::persistAndUnload)

    /** What `/setidletimeout` last set. Recorded and reported; nothing disconnects on it yet. */
    private var idleTimeoutMinutes: Int = 0

    /**
     * Whether the whitelist is enforced right now. `/whitelist on` moves it for this run;
     * `server.toml` decides what it starts as, so a restart follows the file again.
     */
    private var whitelistEnforced: Boolean = config.whitelist

    public val commands: CommandDispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = playerRegistry,
                groupsProvider = { live.current.groups },
                worldSeed = worldSeed,
                maxPlayers = config.maxPlayers,
                stopServer = ::shutdown,
                lang = lang,
                defaultWorldName = defaultWorld.name,
                playerRepository = playerStore.players,
                access = playerStore.access,
                worlds = worlds,
                placeholders = placeholders,
                commandsProvider = { live.current.commands },
                timings = timings,
                blockTagMembers = { tag -> blockTags.membersOf(tag.toString()).ifEmpty { null } },
                itemTagMembers = { tag -> itemTags.membersOf(tag.toString()).ifEmpty { null } },
                entityTagMembers = { tag -> entityTags.membersOf(tag).ifEmpty { null } },
                scoreboard = scoreboard,
                timelines = timelines,
                teams = teams,
                advancements = advancements,
                progress = playerProgress,
                advancementTriggers = advancementTriggers,
            ),
        ).also(::registerVanillaCommands)
            .also { it.register(ReloadCommand(live, ::onConfigsReloaded, functions::reload)) }
            .also { it.register(RestartCommand(::requestRestart)) }
            .also { it.register(BossBarCommand(bossBars)) }
            .also { it.register(ScoreboardCommand(scoreboard)) }
            .also { it.register(TriggerCommand(scoreboard)) }
            .also {
                it.register(
                    ExecuteCommand(scoreboard, bossBars, DataAccess(commandStorage), functionRunner, playerRegistry),
                )
            }.also {
                it.register(
                    DebugCommand(
                        timings,
                        timingsReport,
                        nowNanos = { serverClock.elapsedNow().inWholeNanoseconds },
                        ticksRun = { tickLoop.ticksRun },
                        functions = functionRunner,
                    ),
                )
            }.also { it.register(PerfCommand(timings, timingsReport) { serverClock.elapsedNow().inWholeNanoseconds }) }
            .also { it.register(JfrCommand(timings)) }
            .also { it.register(PublishCommand { config.port }) }
            .also { it.register(UnpublishCommand()) }
            .also { it.register(StopwatchCommand(stopwatches)) }
            .also { it.register(EffectCommand()) }
            .also { it.register(FetchProfileCommand(::fetchMojangUuid)) }
            .also { it.register(FunctionCommand(functions, functionRunner, DataAccess(commandStorage))) }
            .also { it.register(ReturnCommand(functionRunner)) }
            .also { it.register(ScheduleCommand(functions, functionSchedule)) }
            .also { it.register(DataPackCommand(functions)) }
            .also { it.register(SummonCommand(playerRegistry)) }
            .also { it.register(DamageCommand(serverJar)) }
            .also { it.register(RideCommand()) }
            .also { it.register(RotateCommand()) }
            .also { it.register(SwingCommand()) }
            .also { it.register(AttributeCommand()) }
            .also { it.register(ParticleCommand()) }
            .also { it.register(AdvancementCommand(advancements, playerProgress)) }
            .also { it.register(RecipeCommand(playerProgress, recipes)) }
            .also { it.register(LocateCommand()) }
            .also { it.register(WaypointCommand(serverJar)) }
            .also { it.register(PlaceCommand(structureTemplates)) }
            .also { it.register(EnchantCommand(enchantments)) }
            .also { it.register(DialogCommand(dialogs)) }
            .also { it.register(TestCommand(testInstances, functionRunner)) }
            .also { it.register(ItemCommand()) }
            .also { it.register(LootCommand(lootTables) { worldSeed }) }
            .also { it.register(FillBiomeCommand()) }
            .also { it.register(DataCommand(commandStorage)) }
            .also { it.register(SpawnPointCommand()) }
            .also { it.register(RandomCommand { worldSeed }) }
            .also { it.register(TransferCommand()) }
            .also { it.register(BanCommand()) }
            .also { it.register(BanIpCommand()) }
            .also { it.register(BanListCommand()) }
            .also { it.register(PardonCommand(ip = false)) }
            .also { it.register(PardonCommand(ip = true)) }
            .also {
                it.register(
                    WhitelistCommand(
                        enforced = { whitelistEnforced },
                        setEnforced = { value -> whitelistEnforced = value },
                    ),
                )
            }.also { it.register(WorldBorderCommand()) }
            .also { it.register(ForceLoadCommand()) }
            .also { it.register(SpreadPlayersCommand()) }
            .also { it.register(SpectateCommand()) }
            .also { it.register(TickCommand(tickLoop, tickDurations) { serverClock.elapsedNow().inWholeNanoseconds }) }
            .also { it.register(TeamCommand(teams)) }
            .also { it.register(TeamMsgCommand(teams)) }
            .also { it.register(SaveAllCommand(saveControl)) }
            .also { it.register(SaveOffCommand(saveControl)) }
            .also { it.register(SaveOnCommand(saveControl)) }
            .also { it.register(SetIdleTimeoutCommand { minutes -> idleTimeoutMinutes = minutes }) }
            .also {
                it.register(
                    TimingsCommand(
                        timings = timings,
                        tickDurations = tickDurations,
                        report = timingsReport,
                        nowNanos = { serverClock.elapsedNow().inWholeNanoseconds },
                    ),
                )
            }

    /**
     * The server list, rebuilt per ping from `server_list.toml`.
     *
     * The online count comes from the registry rather than from a counter kept alongside it: a
     * second source of truth for "how many players are connected" is how a server ends up
     * advertising numbers that never come back down.
     */
    private val serverList =
        ServerListStatus(
            config = { live.current.serverList },
            placeholders = placeholders,
            slots = config.maxPlayers,
            versionName = BuildInfo.MINECRAFT_VERSION,
            protocolVersion = BuildInfo.PROTOCOL_VERSION,
            onlinePlayers = {
                playerRegistry.snapshot().map { ListedPlayer(it.name, it.uuid.toString()) }
            },
            lookupPlayerByAddress = playerStore.players::findNameByAddress,
        )

    /**
     * What has to be told that the configuration changed.
     *
     * The tab list, scoreboard, boss bar and server list all read their file on every use, so they
     * need nothing. The command tree does not: a command's permission and aliases are baked in when
     * it is registered, and the client holds its own copy for autocompletion, so both have to be
     * built again and sent out.
     */
    private fun onConfigsReloaded() {
        commands.rebuild()
        for (player in playerRegistry.snapshot()) {
            runCatching { player.connection.sendCommandTree() }
        }
    }

    private val defaultGamemode =
        Gamemode.entries.firstOrNull { it.name.equals(config.gamemode, ignoreCase = true) } ?: Gamemode.SURVIVAL

    private val difficulty =
        Difficulty.entries.firstOrNull { it.name.equals(defaultWorld.settings.difficulty, ignoreCase = true) }
            ?: Difficulty.EASY

    /**
     * Live bytes after the last collection, or null if nothing has been collected yet.
     *
     * The chunk counts elsewhere only account for what the server means to hold; if memory climbs
     * while they stay flat, the answer is here, in the gap between what is live and what has been
     * allocated and not yet collected.
     */
    @OptIn(kotlin.native.runtime.NativeRuntimeApi::class, ExperimentalStdlibApi::class)
    private fun liveHeapBytes(): Long? =
        kotlin.native.runtime.GC.lastGCInfo
            ?.memoryUsageAfter
            ?.get("heap")
            ?.totalObjectsSizeBytes

    /**
     * Forces a collection first, so the figure is what is reachable *now* rather than whatever the
     * last scheduled GC saw mid-generation, transient worker state included. Only for the explicit
     * `/timings` report: a forced collection pauses the world and has no place on a placeholder path.
     */
    @OptIn(kotlin.native.runtime.NativeRuntimeApi::class)
    private fun collectedLiveHeapBytes(): Long? {
        // Twice, not once: measured in isolation, a single collection leaves tens of KB per
        // recently generated chunk unswept, and the second pass reclaims it. Only worth paying on
        // the explicit report path.
        kotlin.native.runtime.GC
            .collect()
        kotlin.native.runtime.GC
            .collect()
        return liveHeapBytes()
    }

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
        for (world in worlds.values) {
            world.saveDirty()
            // Clock, weather and game rules, which no chunk carries: cheap enough to write on every
            // sweep, and losing an hour of world time to a crash is worse than one small file write.
            runCatching { commandStorage.save() }
                .onFailure { logger.error(it) { "Could not save command storage" } }
            runCatching { world.saveMetadata() }
                .onFailure { logger.error(it) { "Could not save level.nbt for world '${world.name}'" } }
        }
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
     * The UUID Mojang has for a name, or null when there is none or the lookup failed.
     *
     * Online mode is not required for this: `/fetchprofile` is how an operator finds out who a name
     * belongs to before letting them in, which matters most on a server that is not checking.
     */
    private fun fetchMojangUuid(name: String): String? =
        runCatching { MojangProfileLookup.uuidOf(name) }
            .onFailure { logger.debug { "Profile lookup for '$name' failed: ${it.message}" } }
            .getOrNull()

    /**
     * Everything the world itself owes per tick, plus the termination check.
     *
     * Dropped-item physics lives here rather than on a connection thread so an item falls at one
     * speed regardless of how many players are watching it, and so it keeps falling when nobody is.
     * Nothing here touches a socket, which is the rule this thread lives by (see [TickLoop]):
     * telling clients what moved is each [ClientSession]'s own job.
     */
    private fun tickWorlds() {
        // Scheduled functions first, so one that changes the world does it before this tick's own
        // work reads it. parity: vanilla ticks its function timer before the levels.
        for (id in functionSchedule.tick()) {
            runCatching { functionRunner.run(commands, consoleSource, id) }
                .onFailure { logger.error(it) { "Scheduled function '$id' failed" } }
        }
        for (world in worlds.values) {
            runCatching { world.state.tickItems() }
                .onFailure { logger.error(it) { "Item tick failed in world '${world.name}'" } }
            runCatching { tickTime(world) }
                .onFailure { logger.error(it) { "Time tick failed in world '${world.name}'" } }
            runCatching { tickBorderDamage(world) }
                .onFailure { logger.error(it) { "Border damage failed in world '${world.name}'" } }
            runCatching { tickPortals(world) }
                .onFailure { logger.error(it) { "Portal tick failed in world '${world.name}'" } }
            runCatching { tickSubmersion(world) }
                .onFailure { logger.error(it) { "Drowning tick failed in world '${world.name}'" } }
            runCatching { tickFluids(world) }
                .onFailure { logger.error(it) { "Fluid tick failed in world '${world.name}'" } }
            runCatching { tickEffects(world) }
                .onFailure { logger.error(it) { "Effect tick failed in world '${world.name}'" } }
            runCatching { tickSleep(world) }
                .onFailure { logger.error(it) { "Sleep tick failed in world '${world.name}'" } }
        }
        shutdownIfSignalled()
    }

    /**
     * Skips the night when enough of the world's players have held a bed for five seconds.
     *
     * parity: `ServerLevel.tickTime`'s sleep path, `players_sleeping_percentage` and all: the clock
     * jumps to the next morning, the rain stops and everyone gets up. Nobody is woken by daylight
     * alone, because that is what getting up at dawn already does.
     */
    private fun tickSleep(world: LoadedWorld) {
        val players = playerRegistry.snapshot().filter { it.worldName == world.name }
        val awake = players.filter { it.gamemode != Gamemode.SPECTATOR }
        if (awake.isEmpty()) return
        val sleeping = awake.filter { it.sleepingBed != null }
        if (sleeping.isEmpty()) return
        val percentage = world.gameRules[GameRuleRegistry.PLAYERS_SLEEPING_PERCENTAGE]
        val needed = maxOf(1, (awake.size * percentage + 99) / 100)
        if (sleeping.size < needed) return
        if (sleeping.any { it.sleepTicks < Beds.SLEEP_TICKS_BEFORE_MORNING }) return

        world.clock.setTotalTicks(Beds.nextMorning(world.clock.totalTicks))
        world.weather.raining = false
        world.weather.thundering = false
        for (player in players) {
            player.connection.wakeUp()
            player.connection.sendTime(world)
        }
    }

    /**
     * Hurts whoever has walked outside the border. parity: `ServerPlayer.tick`, which checks every
     * ten ticks rather than every one, so the damage arrives in visible steps.
     */
    private fun tickBorderDamage(world: LoadedWorld) {
        if (world.gameTime % BORDER_DAMAGE_INTERVAL_TICKS != 0L) return
        val border = world.border
        if (border.damagePerBlock <= 0.0) return
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            if (player.gamemode == Gamemode.CREATIVE || player.gamemode == Gamemode.SPECTATOR) continue
            val damage = border.damageAt(player.x, player.z)
            if (damage <= 0f) continue
            player.connection.hurt(damage, "outOfWorldBorder", DamageTypes.OUTSIDE_BORDER)
        }
    }

    /**
     * Moves whoever has stood in a portal long enough, and counts down the wait for everyone else.
     *
     * parity: `Entity.handlePortal`. The delay is the two game rules (nothing in creative, four
     * seconds otherwise), and the 300 tick cooldown afterwards is what stops the portal on the
     * other side from sending them straight back.
     */
    private fun tickPortals(world: LoadedWorld) {
        val creativeDelay = world.gameRules[GameRuleRegistry.PLAYERS_NETHER_PORTAL_CREATIVE_DELAY]
        val defaultDelay = world.gameRules[GameRuleRegistry.PLAYERS_NETHER_PORTAL_DEFAULT_DELAY]
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            if (player.portalCooldown > 0) {
                player.portalCooldown--
                player.portalTicks = 0
                continue
            }
            val state =
                world.state.getBlockState(
                    floor(player.x).toInt(),
                    floor(player.y).toInt(),
                    floor(player.z).toInt(),
                )
            if (state != null && EndPortals.isEndPortal(state)) {
                // parity: `Portal.getPortalTransitionTime` is zero for an end portal, so it takes
                // whoever stepped in on the tick they stepped in.
                travelThroughEndPortal(world, player)
                continue
            }
            val axis = state?.let(PortalShape::axisOf)
            if (axis == null) {
                player.portalTicks = 0
                continue
            }
            player.portalTicks++
            val delay = if (player.gamemode == Gamemode.CREATIVE) creativeDelay else defaultDelay
            if (player.portalTicks < delay) continue
            travelThroughPortal(world, player, axis)
        }
    }

    /**
     * Drowning, burning, and the two numbers the HUD draws them from.
     *
     * parity: `LivingEntity.baseTick` and `Entity.lavaHurt`. The client renders the water overlay
     * from its own copy of the blocks, but the bubbles and the flames come from here: it draws them
     * from the air supply and the shared flags the server sends, and neither moves on its own.
     */
    private fun tickSubmersion(world: LoadedWorld) {
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            if (player.dead) continue
            val eyeY = player.y + Fluids.eyeHeight(player.sneaking)
            val feet =
                world.state.getBlockState(
                    floor(player.x).toInt(),
                    floor(player.y).toInt(),
                    floor(player.z).toInt(),
                )
            val eyes = world.state.getBlockState(floor(player.x).toInt(), floor(eyeY).toInt(), floor(player.z).toInt())
            val air = player.airSupply
            val fire = player.fireTicks

            val drowns = !player.invulnerable && eyes != null && Fluids.of(eyes) == Submersion.WATER
            if (drowns) {
                player.airSupply = Fluids.airAfterTick(player.airSupply, eyesInWater = true)
                if (player.airSupply <= Fluids.DROWNING_AIR) {
                    player.airSupply = 0
                    player.connection.hurt(Fluids.DROWN_DAMAGE, "drown", DamageTypes.DROWN)
                }
            } else {
                player.airSupply = Fluids.airAfterTick(player.airSupply, eyesInWater = false)
            }

            val inLava =
                !player.invulnerable &&
                    (
                        (feet != null && Fluids.of(feet) == Submersion.LAVA) ||
                            (eyes != null && Fluids.of(eyes) == Submersion.LAVA)
                    )
            if (inLava) {
                player.fireTicks = Fluids.LAVA_FIRE_TICKS
                if (world.gameTime % LAVA_DAMAGE_INTERVAL_TICKS == 0L) {
                    player.connection.hurt(Fluids.LAVA_DAMAGE, "lava", DamageTypes.LAVA)
                }
            } else if (player.fireTicks > 0) {
                player.fireTicks--
                // parity: `Entity.baseTick` burns for one heart every second, and lava's own damage
                // replaces it while standing in it.
                if (player.fireTicks % FIRE_DAMAGE_INTERVAL_TICKS == 0) {
                    player.connection.hurt(Fluids.FIRE_DAMAGE, "onFire", DamageTypes.ON_FIRE)
                }
            }

            if (player.airSupply != air || (player.fireTicks > 0) != (fire > 0)) {
                player.connection.syncSubmersion()
            }
        }
    }

    /**
     * Counts every status effect down and applies the ones that act on a tick.
     *
     * parity: `LivingEntity.tickEffects` plus each effect's own `applyEffectTick`, whose intervals
     * are the halvings by amplifier that make a stronger regeneration heal faster.
     */
    private fun tickEffects(world: LoadedWorld) {
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name || player.dead) continue
            if (player.effects.isEmpty()) continue
            for (active in player.effects.values.toList()) {
                applyEffectTick(player, active)
                if (active.infinite) continue
                active.remainingTicks--
                if (active.remainingTicks <= 0) {
                    player.effects.remove(active.effect)
                    player.connection.sendEffectRemoved(active.effect)
                }
            }
        }
    }

    private fun applyEffectTick(
        player: PlayerSession,
        active: ActiveEffect,
    ) {
        val ticks = active.remainingTicks
        when (active.effect) {
            MobEffects.REGENERATION ->
                if (onInterval(ticks, REGENERATION_INTERVAL, active.amplifier) &&
                    player.health < PlayerSession.MAX_HEALTH
                ) {
                    player.connection.heal(1f)
                }
            MobEffects.POISON ->
                if (onInterval(ticks, POISON_INTERVAL, active.amplifier) && player.health > 1f) {
                    player.connection.hurt(1f, "magic", DamageTypes.MAGIC)
                }
            MobEffects.WITHER ->
                if (onInterval(ticks, WITHER_INTERVAL, active.amplifier)) {
                    player.connection.hurt(1f, "wither", DamageTypes.WITHER)
                }
            MobEffects.HUNGER -> player.food.addExhaustion(HUNGER_EXHAUSTION * (active.amplifier + 1))
            else -> Unit
        }
    }

    /** parity: `shouldApplyEffectTickThisTick`, whose interval halves with every level. */
    private fun onInterval(
        ticks: Int,
        base: Int,
        amplifier: Int,
    ): Boolean {
        val interval = base shr amplifier
        return interval <= 0 || ticks % interval == 0
    }

    /**
     * Runs every fluid tick that came due and tells the players what moved.
     *
     * parity: `ServerLevel.tickFluid`, driven by the same per-chunk queue vanilla persists. A tick
     * is scheduled by whatever changed a block next to a fluid, so a settled ocean costs nothing.
     */
    private fun tickFluids(world: LoadedWorld) {
        val due = world.state.dueFluidTicks()
        if (due.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (position in due) {
            for (change in FluidFlow.tick(world, position[0], position[1], position[2])) {
                val pos = BlockPos(change[0], change[1], change[2])
                dropWashedAwayBlock(world, pos, change[4])
                for (player in watchers) {
                    player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                        ClientboundBlockUpdatePacket(pos, change[3]).write(it)
                    }
                }
            }
        }
    }

    /**
     * Drops what a flow washed away. parity: `WaterFluid.beforeDestroyingBlock`, which rolls the
     * block's own loot table exactly as breaking it by hand would.
     *
     * Lava is the exception and drops nothing: its `beforeDestroyingBlock` only plays the fizz.
     */
    private fun dropWashedAwayBlock(
        world: LoadedWorld,
        pos: BlockPos,
        destroyedState: Int,
    ) {
        if (destroyedState <= 0 || FluidFlow.kindOf(destroyedState) != null) return
        if (FluidFlow.kindOf(world.state.getBlockState(pos.x, pos.y, pos.z) ?: return) != FluidKind.WATER) return
        val block = Blocks.byStateId(destroyedState)?.block ?: return
        val table = lootTables.forBlock(block) ?: return
        for (stack in table.generate(LegacyRandom(Random.nextLong()))) {
            if (Items.REGISTRY.byIdentifier(stack.item) == null) continue
            world.state.spawnItem(
                WorldState.DroppedItem(
                    entityId = playerRegistry.allocateEntityId(),
                    uuid = Uuid.random(),
                    item = stack.item,
                    count = stack.count,
                    x = pos.x + 0.5,
                    y = pos.y + 0.5,
                    z = pos.z + 0.5,
                ),
            )
        }
    }

    /** The other side of a Nether portal: this world's own Nether, or the overworld it belongs to. */
    private fun netherCounterpart(world: LoadedWorld): LoadedWorld? {
        val wanted =
            if (world.declaration.dimension == WorldDimension.NETHER) {
                WorldDimension.OVERWORLD
            } else {
                WorldDimension.NETHER
            }
        return counterpart(world, wanted)
    }

    /**
     * The world a portal out of [world] leads to, or null with a line in the log saying why.
     *
     * The pairing comes from `worlds.toml`: a Nether or an End names the Overworld it belongs to,
     * so a server can have several sets of dimensions and a portal still knows which one it is in.
     * A dimension nobody declared leaves the portal leading nowhere, and saying nothing about it is
     * the one answer that leaves the operator guessing. Warned once per world and dimension, not
     * once per tick: a player standing in a portal asks this twenty times a second.
     */
    private fun counterpart(
        world: LoadedWorld,
        dimension: WorldDimension,
    ): LoadedWorld? {
        val declaration = worldsConfig.portalTarget(world.declaration, dimension)
        val target = declaration?.let { worlds[it.name] }
        if (target == null && warnedMissingDimensions.add(world.name to dimension)) {
            val overworld = worldsConfig.overworldOf(world.declaration).name
            logger.warn {
                "A portal in '${world.name}' leads to the ${dimension.configName} of '$overworld', " +
                    "and worlds.toml declares none, so it leads nowhere. Declare one with " +
                    "linked_to = \"$overworld\" to travel through it."
            }
        }
        return target
    }

    /** Which missing dimensions have already been reported, per world. See [counterpart]. */
    private val warnedMissingDimensions = mutableSetOf<Pair<String, WorldDimension>>()

    private fun travelThroughPortal(
        world: LoadedWorld,
        player: PlayerSession,
        axis: PortalAxis,
    ) {
        val target = netherCounterpart(world) ?: return
        // parity: `Level.isAllowedToEnterPortal`, the game rule that keeps a server out of the Nether.
        if (target.declaration.dimension == WorldDimension.NETHER &&
            !world.gameRules[GameRuleRegistry.ALLOW_ENTERING_NETHER_USING_PORTALS]
        ) {
            return
        }
        val exit = NetherPortals.exitFor(world, target, player.x, player.y, player.z, axis) ?: return
        player.portalTicks = 0
        player.portalCooldown = NetherPortals.TRAVEL_COOLDOWN_TICKS
        player.connection.switchWorld(target, exit.x, exit.y, exit.z)
    }

    /** The other side of an end portal: the End, or the world its own spawn is in. */
    private fun endCounterpart(world: LoadedWorld): LoadedWorld? {
        val wanted =
            if (world.declaration.dimension == WorldDimension.END) WorldDimension.OVERWORLD else WorldDimension.END
        return counterpart(world, wanted)
    }

    private fun travelThroughEndPortal(
        world: LoadedWorld,
        player: PlayerSession,
    ) {
        val target = endCounterpart(world) ?: return
        val exit = EndPortals.exitFor(world, target)
        player.portalTicks = 0
        player.portalCooldown = NetherPortals.TRAVEL_COOLDOWN_TICKS
        player.connection.switchWorld(target, exit.x, exit.y, exit.z)
    }

    /**
     * Advances one world's clock and weather, and tells its players about it once a second.
     *
     * parity: vanilla resyncs the clock every 20 ticks rather than every tick, because the client
     * runs its own between updates; sending 20 packets a second per player would be pure noise.
     */
    private fun tickTime(world: LoadedWorld) {
        val wasRaining = world.weather.raining
        val wasThundering = world.weather.thundering
        world.tickTime()
        if (world.weather.raining != wasRaining || world.weather.thundering != wasThundering) {
            commands.environment.syncWeather(world)
        }
        if (world.gameTime % TIME_SYNC_INTERVAL_TICKS == 0L) commands.environment.syncTime(world)
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
        logger.info { if (shouldRestart) "Restarting the server." else "Stopping the server." }
        shutdownComplete.value = 1
    }

    /**
     * Stops this instance so that a fresh one can take its place **in the same process**.
     *
     * Nothing is re-executed and no new process is spawned: everything this instance owns is wound
     * down exactly as `/stop` winds it down, and the caller then builds another one. That is what
     * makes `server.toml` and `worlds.toml` applicable, which `/reload` cannot do: the port is
     * rebound and the worlds are loaded again from scratch, because they really are new.
     */
    internal fun requestRestart() {
        restarting.value = 1
        shutdown()
    }

    @OptIn(ExperimentalTime::class)
    public fun start(startedAt: Instant = Clock.System.now()) {
        // The driver opens lazily, so without a query here a bad path or an unreadable database
        // would not be discovered until the first player joined, which is the worst possible moment
        // to find out. It also creates the file, so the layout is complete before anyone connects.
        logger.info { "Player database ready: ${playerStore.players.count()} player(s) known." }
        val listener = TcpListener.bind(config.port).also { this.listener = it }
        tickLoop.start()
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
                    serverList::statusFor,
                    onlineMode = onlineMode,
                    playerRegistry = playerRegistry,
                    worlds = worlds,
                    defaultWorld = defaultWorld,
                    commands = commands,
                    defaultGamemode = defaultGamemode,
                    difficulty = difficulty,
                    playerStore = playerStore,
                    rememberAddress = if (live.current.serverList.rememberIp) connection.remoteAddress else null,
                    ui = { live.current.ui },
                    placeholders = placeholders,
                    timings = timings,
                    lootTables = lootTables,
                    recipes = recipes,
                    bossBars = bossBars,
                    whitelistEnforced = { whitelistEnforced },
                    enchantments = enchantments,
                    dialogs = dialogs,
                    timelines = timelines,
                    progress = playerProgress,
                    advancementTriggers = advancementTriggers,
                    blockTags = blockTags,
                    fluidTags = fluidTags,
                    timelineTags = timelineTags,
                    damageTypeTags = damageTypeTags,
                    entityTypeTags = entityTags,
                    itemTags = itemTags,
                ).run()
            }
        }

        awaitShutdown()
    }

    /**
     * Keeps `main` alive while a shutdown finishes on another thread.
     *
     * [shutdown] runs on whoever asked for it (the console thread, or the tick loop on a signal).
     * Its very first act is to close the listener, which unblocks the `accept()` above and drops out
     * of the loop, so without this wait the caller carries on *while the save is still running*.
     * That is not hypothetical: it silently truncated the shutdown after "Saving the world.", which
     * is how `players.db` was being left with all of its rows still in the write-ahead log. On a
     * restart it matters twice over, since the next instance would otherwise open the database and
     * bind the port while this one is still closing them.
     */
    private fun awaitShutdown() {
        if (shuttingDown.value == 0) return
        val deadline = TimeSource.Monotonic.markNow()
        while (deadline.elapsedNow().inWholeMilliseconds < SHUTDOWN_WAIT_TIMEOUT_MILLIS) {
            if (shutdownComplete.value == 1) return
            sleepMillis(20)
        }
        logger.error { "Shutdown did not finish within ${SHUTDOWN_WAIT_TIMEOUT_MILLIS}ms, carrying on anyway." }
    }

    private companion object {
        /** parity: `Entity.lavaHurt`, which hurts every ten ticks, and fire, which does every twenty. */
        const val LAVA_DAMAGE_INTERVAL_TICKS = 10L

        // parity: each effect's own `shouldApplyEffectTickThisTick`, halved per level.
        const val REGENERATION_INTERVAL = 50
        const val POISON_INTERVAL = 25
        const val WITHER_INTERVAL = 40
        const val HUNGER_EXHAUSTION = 0.005f
        const val FIRE_DAMAGE_INTERVAL_TICKS = 20

        // 1 declared a view/simulation distance of 1 chunk to the client, below what a real
        // client expects and handles well (vanilla's own slider does not even go that low): it
        // looked like "only one chunk renders" even though every chunk actually had real terrain
        // (confirmed directly by reading the saved region files back). 3 keeps startup bounded
        // while staying in the range a real client behaves normally at.
        const val SPAWN_RADIUS_CHUNKS = 3

        // Beside worlds/, never inside one.
        const val PLAYERS_DATABASE = "players.db"

        // parity: vanilla's own clock resync interval, once a second.
        const val TIME_SYNC_INTERVAL_TICKS = 20L

        // parity: `ServerPlayer.tick` checks the border every ten ticks, not every one.
        const val BORDER_DAMAGE_INTERVAL_TICKS = 10L

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
