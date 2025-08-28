package org.basaltmc.server.runtime

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.lua.LuaLimits
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.plugins.PluginHost
import org.basaltmc.server.plugins.PluginPermission
import org.basaltmc.server.plugins.PluginSurface
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.command.ConsoleCommandSource
import org.basaltmc.server.runtime.command.commands.BossBarRegistry
import org.basaltmc.server.runtime.command.commands.CommandStorage
import org.basaltmc.server.runtime.command.commands.FunctionRunner
import org.basaltmc.server.runtime.command.commands.FunctionSchedule
import org.basaltmc.server.runtime.command.commands.SaveControl
import org.basaltmc.server.runtime.command.commands.Stopwatches
import org.basaltmc.server.runtime.config.BackwardsSelection
import org.basaltmc.server.runtime.config.BasaltConfig
import org.basaltmc.server.runtime.config.CompatibilityConfig
import org.basaltmc.server.runtime.config.ForwardingConfig
import org.basaltmc.server.runtime.config.LiveConfigs
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldsConfig
import org.basaltmc.server.runtime.entity.EntityNbt
import org.basaltmc.server.runtime.entity.Minecarts
import org.basaltmc.server.runtime.entity.MobCombat
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.PrimedTnt
import org.basaltmc.server.runtime.entity.Projectiles
import org.basaltmc.server.runtime.entity.ai.PlayerView
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.server.runtime.placeholder.AnimationPlaceholders
import org.basaltmc.server.runtime.placeholder.PlayerPlaceholders
import org.basaltmc.server.runtime.placeholder.ServerFacts
import org.basaltmc.server.runtime.placeholder.ServerPlaceholders
import org.basaltmc.server.runtime.player.Difficulty
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.plugins.DatabasePluginStorage
import org.basaltmc.server.runtime.plugins.LoadedWorlds
import org.basaltmc.server.runtime.plugins.NpcRegistry
import org.basaltmc.server.runtime.plugins.PluginsCommand
import org.basaltmc.server.runtime.plugins.RegistryPlaceholders
import org.basaltmc.server.runtime.plugins.RegistryPlayers
import org.basaltmc.server.runtime.plugins.ServerPluginServices
import org.basaltmc.server.runtime.plugins.SessionChannels
import org.basaltmc.server.runtime.plugins.SessionMenus
import org.basaltmc.server.runtime.plugins.SessionPackets
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.server.runtime.timings.ReportSources
import org.basaltmc.server.runtime.timings.TimingsReport
import org.basaltmc.server.runtime.world.AdvancementTriggers
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.DetectorRails
import org.basaltmc.server.runtime.world.EndPodium
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.RecipeBook
import org.basaltmc.server.runtime.world.SpawnEquipmentFinalizer
import org.basaltmc.server.runtime.world.VanillaWorldGeneratorFactory
import org.basaltmc.server.runtime.world.Vibrations
import org.basaltmc.server.runtime.world.WORLDS_ROOT
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.world.GenerationMetrics
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
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.multiversion.UnknownProtocolPolicy
import org.basaltmc.wire.multiversion.java.JavaTranslators
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.SoundSource
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

/**
 * A thread-per-connection server covering Handshake, Status, Login, Configuration and Play for a
 * single world. [worldGeneratorFactory], [overworldRouter] and [surfaceEnvironment] stay optional
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
    private val spawnEquipment: SpawnEquipmentFinalizer = SpawnEquipmentFinalizer(serverJar),
    private val worldsConfig: WorldsConfig,
    /**
     * Which client versions may in. Read once: it decides what a connection is before there is a
     * player, so it is not one of the files `/reload` swaps.
     */
    private val compatibility: CompatibilityConfig =
        CompatibilityConfig(
            BackwardsSelection.All,
            UnknownProtocolPolicy.REFUSE,
            UnknownProtocolPolicy.REFUSE,
        ),
    /** Where `plugins/` is. The working directory on a real server; under `build/` in a test. */
    private val pluginsRoot: String = ".",
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
    private val playersDatabasePath: String = PLAYERS_DATABASE,
    private val worldsRoot: String = WORLDS_ROOT,
) {
    private val logger = Logger("Server")
    private val playerRegistry = PlayerRegistry()

    /** Built once from the configuration and what this build can translate. */
    private val protocolGate = compatibility.gate(JavaTranslators.supported, logger)

    /** Null when the operator turned the naming off, which is what the rewriter reads as "leave it". */
    private val substitutionLabels = if (compatibility.nameSubstitutes) SubstituteLabels() else null

    /** Player-shaped entities a plugin made, drawn by every connection that may see them. */
    private val npcs = NpcRegistry()
    private val cpuMeter = CpuMeter()

    /** Every table read straight out of the operator's own `server.jar`. See [VanillaGameData]. */
    private val vanillaData = VanillaGameData(serverJar, logger)

    /** The chunks each tick method walks that are meant to be simulated, not merely loaded. */
    private val chunkSimulation = ChunkSimulationView(playerRegistry)

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
            advancements = vanillaData.advancements,
            progress = playerProgress,
            itemTagMembers = { tag -> vanillaData.itemTags.membersOf(tag.toString()) },
            announce = { _, message ->
                if (worldRegistry.defaultWorld.gameRules[GameRuleRegistry.SHOW_ADVANCEMENT_MESSAGES]) {
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
                    RecipeBook.add(it, vanillaData.recipes, listOf(recipe))
                }
            },
        )

    /** Native heap figures for `/timings` and the server-facts placeholders. See [ServerMetrics]. */
    private val metrics = ServerMetrics()

    /**
     * Every world this server knows about, what makes a new one, and the region scheduler they
     * share. See [WorldRegistry].
     */
    private val worldRegistry =
        WorldRegistry(
            config = config,
            worldsConfig = worldsConfig,
            overworldRouter = overworldRouter,
            surfaceEnvironment = surfaceEnvironment,
            villageGenerator = villageGenerator,
            worldGeneratorFactory = worldGeneratorFactory,
            worldsRoot = worldsRoot,
            logger = logger,
            timings = timings,
            defaultWorldMetadata = defaultWorldMetadata,
            pluginEvents = { plugins.events },
            commands = { commands },
            pluginServices = { pluginServices },
            playerRegistry = playerRegistry,
        )

    init {
        for (world in worldRegistry.worlds.values) {
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
            worldRegistry.defaultWorld.settings.saveIntervalSeconds,
            durations = tickDurations,
            onSaveInterval = {
                timings.of("tick.save").time { if (saveControl.enabled) persistence.persistEverything() }
            },
            onUnloadInterval = { timings.of("tick.unload").time { persistence.unloadUnreachableChunks() } },
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
            register("basalt_player", PlayerPlaceholders({ live.current.groups }) { worldRegistry.worlds })
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
                        loadedChunks = { worldRegistry.worlds.values.sumOf { it.state.loadedChunkCount } },
                        liveHeapBytes = { metrics.liveHeapBytes() },
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
                loadedChunks = { worldRegistry.worlds.values.sumOf { world -> world.state.loadedChunkCount } },
                regionCacheBytes = { worldRegistry.worlds.values.sumOf { it.regions.cachedRegionBytes } },
                chunkBytes = { worldRegistry.worlds.values.sumOf { it.state.chunkMemory().bytes } },
                chunkBlockEntities = { worldRegistry.worlds.values.sumOf { it.state.chunkMemory().blockEntities } },
                chunkScheduledTicks = {
                    worldRegistry.worlds.values.sumOf { it.state.chunkMemory().scheduledTicks }
                },
                chunkGeneratedEntities = {
                    worldRegistry.worlds.values.sumOf { it.state.chunkMemory().generatedEntities }
                },
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
                worldCount = { worldRegistry.worlds.size },
                cpuPercent = cpuMeter::percent,
                liveHeapBytes = { metrics.collectedLiveHeapBytes() },
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

    /** What saves players, worlds and unreachable chunks to disk. See [PersistenceCoordinator]. */
    private val persistence =
        PersistenceCoordinator(
            worlds = { worldRegistry.worlds },
            playerRegistry = playerRegistry,
            playerStore = playerStore,
            commandStorage = commandStorage,
            pluginEvents = { plugins.events },
            metrics = metrics,
            logger = logger,
            spawnRadiusChunks = SPAWN_RADIUS_CHUNKS,
            retainMarginChunks = RETAIN_MARGIN_CHUNKS,
        )

    /** `/save-off` stops the periodic save; `/save-all` runs one on demand. */
    private val saveControl: SaveControl = SaveControl(persistence::persistEverything)

    /** What `/setidletimeout` last set. Recorded and reported; nothing disconnects on it yet. */
    private var idleTimeoutMinutes: Int = 0

    /**
     * Whether the whitelist is enforced right now. `/whitelist on` moves it for this run;
     * `server.toml` decides what it starts as, so a restart follows the file again.
     */
    private var whitelistEnforced: Boolean = config.whitelist

    public val commands: CommandDispatcher =
        buildCommands(
            playerRegistry = playerRegistry,
            pluginEventsProvider = { plugins.events },
            live = live,
            worldSeed = worldRegistry.worldSeed,
            config = config,
            stopServer = ::shutdown,
            lang = lang,
            defaultWorldName = worldRegistry.defaultWorld.name,
            playerStore = playerStore,
            worlds = worldRegistry.worlds,
            placeholders = placeholders,
            timings = timings,
            vanillaData = vanillaData,
            scoreboard = scoreboard,
            teams = teams,
            playerProgress = playerProgress,
            advancementTriggers = advancementTriggers,
            onConfigsReloaded = worldRegistry::onConfigsReloaded,
            functions = functions,
            requestRestart = ::requestRestart,
            protocolGate = protocolGate,
            bossBars = bossBars,
            commandStorage = commandStorage,
            functionRunner = functionRunner,
            timingsReport = timingsReport,
            serverClock = serverClock,
            tickLoop = tickLoop,
            stopwatches = stopwatches,
            fetchMojangUuid = persistence::fetchMojangUuid,
            functionSchedule = functionSchedule,
            serverJar = serverJar,
            whitelistEnforced = { whitelistEnforced },
            setWhitelistEnforced = { value -> whitelistEnforced = value },
            tickDurations = tickDurations,
            saveControl = saveControl,
            setIdleTimeoutMinutes = { minutes -> idleTimeoutMinutes = minutes },
        )

    /** The player handles a plugin holds, over the live registry and the server's own groups. */
    private val pluginPlayers =
        RegistryPlayers(
            registry = playerRegistry,
            groups = { live.current.groups },
            worldFor = { name -> worldRegistry.worlds[name] },
        )

    /** A plugin's placeholders, in the server's own registry rather than one of their own. */
    private val pluginPlaceholders =
        RegistryPlaceholders(placeholders, pluginPlayers).also { it.sessions = { playerRegistry.snapshot() } }

    /**
     * The seam the plugins see the server through.
     *
     * Everything they run goes on the global owner: it is the one owner that exists in both
     * scheduler modes and is a single thread in both, which is what makes "one lane" true today
     * rather than a promise. Lanes follow regions later, and that changes this line and nothing a
     * plugin can see.
     */
    private val pluginServices: ServerPluginServices =
        ServerPluginServices(
            owner = worldRegistry.regionScheduler.global,
            dispatcher = commands,
            currentTick = { tickLoop.ticksRun },
            console = { consoleSource },
            root = pluginsRoot,
            players = pluginPlayers,
            worlds =
                LoadedWorlds(
                    worlds = { worldRegistry.worlds },
                    defaultName = { worldRegistry.defaultWorld.name },
                    spawnMob = { world, type, x, y, z -> spawning.spawnMob(world, type, x, y, z, reason = "plugin") },
                    make = { name, dimension, type, seed -> worldRegistry.createWorld(name, dimension, type, seed) },
                    npcs = npcs,
                    nextEntityId = { playerRegistry.allocateEntityId() },
                    players = { playerRegistry.snapshot() },
                ),
            placeholders = pluginPlaceholders,
            storage = DatabasePluginStorage { playerStore },
            menus = SessionMenus(playerRegistry),
            packets = SessionPackets(playerRegistry),
            channels = SessionChannels(playerRegistry),
            // The owner of a position, which with regions off is the world's one.
            ownerAt = { world, x, z ->
                worldRegistry.worlds[world]?.let {
                    worldRegistry.regionScheduler.regionFor(it.name, x.toInt() shr 4, z.toInt() shr 4).owner
                }
            },
        )

    /**
     * The plugins, with what `[plugins]` in `server.toml` lets them spend.
     *
     * Read once rather than on every use, and not part of what `/reload` swaps: the limits are
     * baked into a plugin's state when it loads, so changing them under a running plugin would mean
     * one thing for the code already running and another for the code about to. Reloading the
     * plugin is what applies them.
     */
    public val plugins: PluginHost =
        PluginHost(
            pluginServices,
            lanes = config.pluginLanes,
            limits =
                LuaLimits(
                    instructionsPerTurn = config.pluginInstructionsPerTurn,
                    memoryBytes = config.pluginMemoryBytes,
                ),
        ).also { host ->
            commands.register(PluginsCommand(host) { playersDatabasePath })
            // A decider answers where the question was asked, so it may only run on the owner
            // plugins live on. With regions off that is every owner, because they all take turns on
            // this one thread; with regions on and more than one thread it is not, and the host says
            // so instead of running Lua on two threads at once.
            // Unclaimed counts as ours, by the owner's own rule: before the first tick nothing else
            // is running, and that is when the worlds announce themselves.
            host.onPluginThread = {
                worldRegistry.regionScheduler.global.isCurrent || !worldRegistry.regionScheduler.global.isClaimed
            }
            host.timings = timings
            host.allowed = { id ->
                config.pluginAllowances[id]
                    .orEmpty()
                    .mapNotNull(PluginPermission::of)
                    .toSet()
            }
        }

    /** Everything a block change owes the clients watching it. See [BlockChangeBroadcaster]. */
    private val broadcaster = BlockChangeBroadcaster(playerRegistry, vanillaData.lootTables, plugins.events)

    /** What an explosion breaks, drops and hurts. See [ExplosionSystem]. */
    private val explosions = ExplosionSystem(playerRegistry, plugins.events, vanillaData.lootTables)

    /** Natural mob spawning, and the one-off spawn a plugin or a raid asks for. See [NaturalSpawningSystem]. */
    private val spawning =
        NaturalSpawningSystem(playerRegistry, plugins.events, vanillaData.biomeSpawns, chunkSimulation, spawnEquipment)

    /** Projectiles in flight and what they hit. See [CombatSystem]. */
    private val combat = CombatSystem(plugins.events, broadcaster, explosions)

    /** Fire, redstone, fluids, random ticks, block entities and falling blocks. See [RedstoneTickSystem]. */
    private val redstone =
        RedstoneTickSystem(playerRegistry, plugins.events, vanillaData.recipes, broadcaster, chunkSimulation)

    /** Raid waves, their boss bar and the bad omen that starts one. See [RaidSystem]. */
    private val raids = RaidSystem(playerRegistry, plugins.events, logger, broadcaster, spawning)

    /** Status effect countdown, handing bad/raid omen off to [raids]. See [StatusEffectSystem]. */
    private val statusEffects = StatusEffectSystem(playerRegistry, tickOmens = raids::tickOmens)

    /** Sleep, the border, drowning and burning, the clock, weather, lightning and portals. See [EnvironmentTickSystem]. */
    private val environment =
        EnvironmentTickSystem(
            playerRegistry,
            plugins.events,
            logger,
            worldsConfig,
            { worldRegistry.worlds },
            chunkSimulation,
            broadcaster,
            commands.environment,
            spawning,
        )

    /**
     * What a goal is allowed to make the world do, the rest of a mob's own turn (fire, AI,
     * breeding) and what it drops on death. See [MobActionSystem].
     */
    private val mobActionSystem =
        MobActionSystem(
            playerRegistry,
            plugins.events,
            broadcaster,
            explosions,
            spawning,
            combat,
            raids,
            vanillaData.lootTables,
            { worldRegistry.worlds },
        )

    private val mobTicks = MobTickSystem(mobActionSystem)

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
            pluginEvents = { plugins.events },
        )

    private val defaultGamemode =
        Gamemode.entries.firstOrNull { it.name.equals(config.gamemode, ignoreCase = true) } ?: Gamemode.SURVIVAL

    private val difficulty =
        Difficulty.entries.firstOrNull {
            it.name.equals(worldRegistry.defaultWorld.settings.difficulty, ignoreCase = true)
        } ?: Difficulty.EASY

    /**
     * Everything the world itself owes per tick, plus the termination check.
     *
     * Dropped-item physics lives here rather than on a connection thread so an item falls at one
     * speed regardless of how many players are watching it, and so it keeps falling when nobody is.
     * Nothing here touches a socket, which is the rule this thread lives by (see [TickLoop]):
     * telling clients what moved is each [ClientSession]'s own job.
     */
    private fun tickWorlds() {
        // Told once a tick, and only if somebody asked: with no listener this is a boolean.
        plugins.events.let { bus ->
            if (bus.listening(PluginEventTypes.SERVER_TICK)) {
                val event = bus.event(PluginEventTypes.SERVER_TICK)
                event.set(0, tickLoop.ticksRun)
                bus.fire(event)
            }
        }
        // Scheduled functions first, so one that changes the world does it before this tick's own
        // work reads it. parity: vanilla ticks its function timer before the levels.
        for (id in functionSchedule.tick()) {
            runCatching { functionRunner.run(commands, consoleSource, id) }
                .onFailure { logger.error(it) { "Scheduled function '$id' failed" } }
        }
        // What the world has loaded decides what its regions are, and it is worked out here, at the
        // tick boundary, before any of them starts: a chunk changing hands mid-tick is the one
        // thing this model cannot survive. With regions off it does nothing.
        for (world in worldRegistry.worlds.values) {
            // Only when the world's chunks actually moved: the shape of a world is decided by them,
            // and walking every loaded chunk on a tick where none arrived or left is pure cost.
            val version = world.state.chunkSetVersion
            if (version == worldRegistry.lastChunkSetVersion[world.name]) continue
            worldRegistry.lastChunkSetVersion[world.name] = version
            worldRegistry.regionScheduler.repartition(world.name, world.state.loadedChunkKeys()) { merged, split ->
                logger.debug { "World '${world.name}': $merged region(s) merged, $split split" }
            }
        }
        // Where each player's packets go is decided here, between turns: no region is running, so
        // handing a session that walked across a border to its new owner cannot land two threads
        // inside the same player.
        for (player in playerRegistry.snapshot()) {
            val world = worldRegistry.worlds[player.worldName] ?: continue
            player.connection.handOverToRegionOf(world)
        }
        // Every turn goes through the scheduler, in both modes: the global owner first and then
        // each region. With the world in one piece that is one region per world, ticked in the
        // order the worlds were loaded, which is the order this loop walked them in before.
        worldRegistry.regionScheduler.tick(
            onFailure = { failure ->
                logger.error(failure) { "A tick task failed: ${failure.stackTraceToString().take(600)}" }
            },
        ) { region ->
            val world = worldRegistry.worlds[region.worldName] ?: return@tick
            val onTaskFailure = { failure: Throwable ->
                logger.error(failure) { "A task for world '${world.name}' failed" }
            }
            // A turn is: what came due, then what was waiting, then the world's own work, and last
            // what asked to run at the end of it. Everything below this line is that third part.
            // This region's own queue, not the world's: once a world is split there are several,
            // and running the first region's queue from every region's thread was two threads in
            // the same queue at once.
            timings.of("tick.tasks").time {
                region.owner.advanceTick()
                region.owner.runPending(onTaskFailure)
            }
            // What belongs to a piece of the map is done by whoever owns that piece; what belongs
            // to the world as a whole (its clock, its weather, its players) runs once, in the
            // world's first region. With one region the two are the same thing.
            runCatching { world.state.tickItems(region) }
                .onFailure { logger.error(it) { "Item tick failed in world '${world.name}'" } }
            if (region.isPrimary) {
                runCatching { environment.tickTime(world) }
                    .onFailure { logger.error(it) { "Time tick failed in world '${world.name}'" } }
                runCatching { environment.tickBorderDamage(world) }
                    .onFailure { logger.error(it) { "Border damage failed in world '${world.name}'" } }
                runCatching { environment.tickPortals(world) }
                    .onFailure { logger.error(it) { "Portal tick failed in world '${world.name}'" } }
                runCatching { environment.tickSubmersion(world) }
                    .onFailure { logger.error(it) { "Drowning tick failed in world '${world.name}'" } }
                runCatching { redstone.tickFootsteps(world) }
                    .onFailure { logger.error(it) { "Footstep tick failed in world '${world.name}'" } }
            }
            runCatching { redstone.tickInhabitedTime(world, region) }
                .onFailure { logger.error(it) { "Inhabited-time tick failed in world '${world.name}'" } }
            runCatching { redstone.tickFluids(world, region) }
                .onFailure { logger.error(it) { "Fluid tick failed in world '${world.name}'" } }
            runCatching { redstone.tickRandomBlocks(world, region) }
                .onFailure { logger.error(it) { "Random block tick failed in world '${world.name}'" } }
            runCatching { redstone.tickPressurePlates(world, region) }
                .onFailure { logger.error(it) { "Pressure plate tick failed in world '${world.name}'" } }
            runCatching { redstone.tickTripwires(world, region) }
                .onFailure { logger.error(it) { "Tripwire tick failed in world '${world.name}'" } }
            runCatching { redstone.tickScheduledBlocks(world, region) }
                .onFailure { logger.error(it) { "Scheduled block tick failed in world '${world.name}'" } }
            runCatching { redstone.tickBlockEntities(world, region) }
                .onFailure { logger.error(it) { "Block entity tick failed in world '${world.name}'" } }
            runCatching { redstone.tickFallingBlocks(world, region) }
                .onFailure { logger.error(it) { "Falling block tick failed in world '${world.name}'" } }
            runCatching { tickEntities(world, region) }
                .onFailure { logger.error(it) { "Entity tick failed in world '${world.name}'" } }
            runCatching { spawning.tickSpawning(world, region) }
                .onFailure { logger.error(it) { "Mob spawning failed in world '${world.name}'" } }
            if (region.isPrimary) {
                runCatching { statusEffects.tickEffects(world) }
                    .onFailure { logger.error(it) { "Effect tick failed in world '${world.name}'" } }
                runCatching { raids.tickRaids(world) }
                    .onFailure { logger.error(it) { "Raid tick failed in world '${world.name}'" } }
                runCatching { environment.tickSleep(world) }
                    .onFailure { logger.error(it) { "Sleep tick failed in world '${world.name}'" } }
            }
            // The end of the turn: what has to see everything it did, rather than see it a tick late.
            timings.of("tick.tasks_end").time { region.owner.runEndOfTick(onTaskFailure) }
        }
        shutdownIfSignalled()
    }

    /**
     * Watches for a plugin that went into the server and did not come back.
     *
     * The instruction ceiling stops one spinning in Lua on its own. This is for the other half: a
     * plugin stuck inside something the host is doing for it, which no counter sees. Its own thread,
     * because a watchdog that runs on the thread it is watching is not a watchdog.
     */
    private fun startPluginWatchdog() {
        NativeThread.start {
            while (accepting.value == 1) {
                sleepMillis(PLUGIN_WATCHDOG_INTERVAL_MILLIS)
                plugins.interruptIfStuck(PLUGIN_STUCK_MILLIS)
            }
        }
    }

    /**
     * Names the plugins that left data behind, once, at startup.
     *
     * Nothing is deleted: a plugin taken out for an afternoon has to find its data when it comes
     * back. The operator is told it is there and how to get rid of it, and can turn the reminder
     * off with `orphan-data = "quiet"`.
     */
    private fun reportOrphanedPluginData() {
        if (config.orphanData != "warn") return
        val orphans = plugins.orphanedData()
        if (orphans.isEmpty()) return
        logger.warn {
            "Data is here for ${orphans.size} plugin(s) that are not installed: " +
                orphans.entries.sortedByDescending { it.value }.joinToString(", ") { "${it.key} (${it.value} rows)" } +
                ". Nothing was deleted. /plugins orphans lists them, /plugins purge <id> removes one."
        }
    }

    /**
     * Ticks the entities that are not players, items or falling blocks: today that is primed TNT.
     *
     * parity: `ServerLevel.tickNonPassenger`, one turn per entity per tick, in the order they were
     * spawned. What each one does is its own class's `tick`.
     */
    private fun tickEntities(
        world: LoadedWorld,
        region: Region,
    ) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        // parity: `TntBlock.explode`, which spawns the entity as the block goes. It is done here
        // because the block half runs wherever a neighbour changed, which may be a client's thread.
        val protocolType = EntityTypes[PrimedTnt.TYPE]?.protocolId
        if (protocolType != null) {
            for (position in world.state.takePrimedTnt()) {
                PrimedTnt.prime(
                    world.state,
                    position[0],
                    position[1],
                    position[2],
                    entityId = playerRegistry.allocateEntityId(),
                    protocolType = protocolType,
                )
                // parity: `TntBlock.explode`, which is where the fuse is heard being lit.
                broadcaster.sendEffects(
                    watchers,
                    position[0],
                    position[1],
                    position[2],
                    Blocks.AIR.stateId,
                    listOf(BlockEffect.Sound(SOUND_TNT_PRIMED, SoundSource.BLOCKS, 1f, 1f)),
                )
            }
        }
        // The mobs a chunk brought back from disk: they wait for the tick loop because an entity id
        // is the server's to hand out, and a chunk is loaded on a generation thread.
        for (tag in world.state.takePendingEntities()) {
            val restored =
                EntityNbt.decodeTree(
                    tag,
                    playerRegistry::allocateEntityId,
                    protocolTypeOf = { type -> EntityTypes[type]?.protocolId },
                )
            restored.forEach(world.state::spawnEntity)
        }
        // Structure templates are generated off-thread and cannot allocate runtime ids there. A
        // mob descriptor becomes the same live ServerEntity as a spawn egg here, so it receives AI,
        // movement, persistence and ordinary client synchronization instead of remaining scenery.
        for (descriptor in world.state.takePendingGeneratedEntities()) {
            val generated =
                EntityNbt.decodeGeneratedTree(
                    descriptor,
                    playerRegistry::allocateEntityId,
                    protocolTypeOf = { type -> EntityTypes[type]?.protocolId },
                )
            generated.forEach(world.state::spawnEntity)
        }
        // parity: `Entity.die`, which drops the entity's own loot table where it stood. Whatever was
        // killed since the last tick is dealt with here, once, before anything else moves.
        for (dead in world.state.entities().filter { it.killed }) {
            dead.killed = false
            mobActionSystem.dropDeathLoot(world, dead)
            // parity: `GameEvent.ENTITY_DIE`, the loudest thing a sculk sensor hears.
            val heard = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
            Vibrations.emit(world, "entity_die", dead.x, dead.y, dead.z, heard)
            for (change in heard) broadcaster.announceRedstone(world, watchers, change)
        }
        // One snapshot for the whole tick: `entities()` takes the world's lock and copies the list,
        // and asking for it per entity turned a pen of cows into a lock convoy.
        val living = world.state.entities().filter { !it.removed }
        // parity: `TargetingConditions.forCombat`, which skips anybody a mob cannot fight. Only the
        // players in this region: anybody else is at least a section away, which is further than any
        // mob's follow range, and building the list for every region was a hundred copies of it.
        val views =
            watchers.filter { region.ownsPosition(it.x, it.z) }.map { player ->
                PlayerView(
                    entityId = player.entityId,
                    uuid = player.uuid.toString(),
                    x = player.x,
                    y = player.y,
                    z = player.z,
                    width = EntityTypeShapes.widthOf(PLAYER_TYPE),
                    height = EntityTypeShapes.heightOf(PLAYER_TYPE),
                    // parity: `TargetingConditions.forCombat`, which skips whoever cannot be fought.
                    targetable =
                        !player.dead &&
                            player.gamemode != Gamemode.CREATIVE &&
                            player.gamemode != Gamemode.SPECTATOR,
                    heldItem =
                        player.inventory.heldItem?.let { runCatching { Identifier.parse(it.id) }.getOrNull() },
                    sneaking = player.sneaking,
                    yaw = player.yaw,
                    pitch = player.pitch,
                )
            }
        val targets =
            watchers
                .filter { !it.dead && it.gamemode != Gamemode.CREATIVE && it.gamemode != Gamemode.SPECTATOR }
                .map {
                    MobCombat.Target(
                        it.entityId,
                        it.x,
                        it.y,
                        it.z,
                        EntityTypeShapes.widthOf(PLAYER_TYPE),
                        EntityTypeShapes.heightOf(PLAYER_TYPE),
                    )
                }
        val random = LegacyRandom(Random.nextLong())
        for (entity in living) {
            if (entity.removed) continue
            if (mobTicks.hasActiveVehicle(world, entity)) continue
            // An entity is ticked by whoever owns the ground it is standing on. It may walk out of
            // this region during its own turn: that is fine, the next tick asks again, and the
            // repartition that could change the answer only happens between ticks.
            if (!region.ownsPosition(entity.x, entity.z)) continue
            if (entity.type == PrimedTnt.TYPE) {
                if (!PrimedTnt.tick(world.state, entity)) continue
                entity.removed = true
                explosions.blowUp(
                    world,
                    watchers,
                    entity.x,
                    entity.y + PrimedTnt.EXPLOSION_OFFSET,
                    entity.z,
                    PrimedTnt.EXPLOSION_POWER,
                    "tnt",
                    entity.entityId,
                )
                // parity: `GameEvent.EXPLODE`, which is fifteen: the top of the scale.
                val heard = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
                Vibrations.emit(world, "explode", entity.x, entity.y, entity.z, heard)
                for (change in heard) broadcaster.announceRedstone(world, watchers, change)
                continue
            }
            val projectile = Projectiles.kindOf(entity.type)
            if (projectile != null) {
                combat.tickProjectile(world, watchers, living, entity, projectile)
                continue
            }
            // parity: `AbstractMinecart.tick`: on a rail it follows the track, and off one it falls.
            if (DetectorRails.isMinecart(entity.type)) {
                // parity: `AbstractMinecart.tick`, where what a cart has taken heals a point a tick.
                if (entity.vehicleDamage > 0f) entity.vehicleDamage -= 1f
                val rider = watchers.firstOrNull { it.entityId in entity.passengerIds }
                // parity: `AbstractMinecart.push`, the shove a cart takes from anybody stood in it.
                for (player in watchers) {
                    if (player.entityId == rider?.entityId) continue
                    if (!mobActionSystem.touchesCart(entity, player)) continue
                    Minecarts.pushedBy(entity, player.x, player.z)
                }
                val intent =
                    rider?.let {
                        Minecarts.intentOf(it.inputForward, it.inputBack, it.inputLeft, it.inputRight, it.yaw)
                    } ?: EMPTY_INTENT
                if (!Minecarts.moveAlongTrack(world.state, entity, intent[0], intent[1], rider != null)) {
                    Minecarts.comeOffTrack(world.state, entity, redstone.minecartGravity)
                }
                // The rider's own copy of where they are comes from the cart: the client draws
                // itself on the cart, and the server needs the same answer to load chunks around it.
                rider?.let {
                    it.x = entity.x
                    it.y = entity.y
                    it.z = entity.z
                }
                val pressed =
                    DetectorRails.check(
                        world,
                        floor(entity.x).toInt(),
                        floor(entity.y).toInt(),
                        floor(entity.z).toInt(),
                        living,
                    )
                pressed?.let { broadcaster.announceRedstone(world, watchers, it) }
                mobTicks.tickPassengers(world, entity, watchers, views, living, random)
                continue
            }
            // parity: `Mob.aiStep`, which is what makes an animal wander rather than stand there.
            if (Mobs.isMob(entity.type)) {
                mobTicks.tickTree(world, entity, watchers, views, living, random)
            } else {
                mobTicks.tickPassengers(world, entity, watchers, views, living, random)
            }
        }
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
     * Disconnects everyone, stops the tick loop, drains the queues, then saves once: saving while
     * still live may never finish, since connected players keep generating more to save.
     */
    private fun shutdown() {
        // Once only: `/stop` and a signal can arrive together, and running this twice means two
        // concurrent saves of the same regions.
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
            persistence.persistPlayers()

            logger.info { "Disconnecting ${players.size} player(s)." }
            for (player in players) {
                runCatching { player.connection.disconnect(Component.text("Server closed", TextColor.RED)) }
                playerRegistry.remove(player.uuid)
            }
        }

        tickLoop.stop()
        plugins.events.emit(PluginEventTypes.SERVER_STOP) { event -> event.set(0, players.size) }
        // After the loop has stopped, for the same reason it loaded before it started.
        runCatching { plugins.unloadAll() }.onFailure { logger.error(it) { "Failed to unload plugins" } }
        for (world in worldRegistry.worlds.values) world.stopGenerating(SHUTDOWN_DRAIN_TIMEOUT_MILLIS)

        logger.info { "Saving ${worldRegistry.worlds.size} world(s)." }
        for (world in worldRegistry.worlds.values) {
            plugins.events.emit(PluginEventTypes.WORLD_SAVE) { event -> event.set(0, world.name) }
            world.saveDirty()
            world.flushRegions()
            // Nothing else runs in this world: what was still queued is cancelled rather than run
            // against a world that is already written out, and its handles say so.
        }
        // Nothing else runs anywhere: what was queued is cancelled rather than run against a world
        // that is already written out.
        worldRegistry.regionScheduler.close()
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
        // Said once at startup, because it changes the order of tick and an operator reading a bug
        // report has to know which mode produced it.
        if (worldRegistry.regionScheduler.regional) {
            logger.warn {
                "Regions are on, which is NOT VANILLA: two of them have no fixed order of tick " +
                    "between each other, and what crosses a border is deferred a tick. " +
                    "Ticking them at once uses ${worldRegistry.regionScheduler.threads} thread(s)."
            }
        }
        // Before the loop, while nothing else is running: a plugin's own code is the last thing to
        // come up and the first to go down, so it never races the tick thread over its own state.
        if (config.pluginsEnabled) {
            plugins.loadAll()
            reportOrphanedPluginData()
            startPluginWatchdog()
        } else {
            logger.info { "Plugins are off in server.toml." }
        }
        // The worlds came up before the plugins did, so this says "these are the worlds" rather
        // than announcing something they could have watched happen. Telling them earlier would mean
        // telling nobody, which is what the first run of this did.
        // On the owner, not on the thread that is still starting the server up: a decider only runs
        // where the plugins do, so firing this here would have been telling nobody a second time.
        // Queued rather than run, so it lands as the first thing the loop does.
        worldRegistry.regionScheduler.global.submit {
            for (world in worldRegistry.worlds.values) {
                plugins.events.emit(PluginEventTypes.WORLD_LOAD) { event -> event.set(0, world.name) }
            }
        }
        for (world in worldRegistry.worlds.values) {
            world.state.onChunkInstalled = { chunk ->
                plugins.events.emit(PluginEventTypes.WORLD_CHUNK_LOAD) { event ->
                    event.set(0, world.name)
                    event.set(1, chunk.chunkX)
                    event.set(2, chunk.chunkZ)
                    event.set(3, true)
                }
            }
        }
        // After the plugins are up, so the first thing a plugin can be told is that everything is
        // there. Nothing before this point is worth telling anyone about: half a server is not one.
        plugins.events.emit(PluginEventTypes.SERVER_LOAD) { event ->
            event.set(0, worldRegistry.worlds.size)
            event.set(1, plugins.statuses.size)
        }
        tickLoop.start()
        val elapsedSeconds = (Clock.System.now() - startedAt).inWholeMilliseconds / 1000.0
        logger.info {
            "Done (${elapsedSeconds}s)! Listening on port ${config.port} " +
                "(${if (onlineMode != null) "online" else "offline"}-mode)."
        }
        ForwardingConfig.announce(config.forwarding, config.onlineMode, config.port, logger)
        val translated = protocolGate.supported - ProtocolVersions.NATIVE
        if (translated.isNotEmpty()) {
            // The range and the count, not the list: forty eight of them is a wall of text nobody
            // reads, and `/version` prints the whole thing for whoever wants it.
            val oldest = translated.min()
            logger.info {
                "Also accepting ${translated.size} older client versions, " +
                    "${ProtocolVersions.describe(oldest)} to ${ProtocolVersions.describe(translated.max())} " +
                    "(protocols $oldest to ${translated.max()})."
            }
        }
        while (accepting.value == 1) {
            val connection = runCatching { listener.accept() }.getOrNull() ?: break
            NativeThread.start {
                ClientSession(
                    connection,
                    serverList::statusFor,
                    onlineMode = onlineMode,
                    playerRegistry = playerRegistry,
                    worlds = worldRegistry.worlds,
                    defaultWorld = worldRegistry.defaultWorld,
                    commands = commands,
                    defaultGamemode = defaultGamemode,
                    difficulty = difficulty,
                    playerStore = playerStore,
                    rememberIp = live.current.serverList.rememberIp,
                    forwarding = config.forwarding,
                    ui = { live.current.ui },
                    placeholders = placeholders,
                    timings = timings,
                    lootTables = vanillaData.lootTables,
                    recipes = vanillaData.recipes,
                    villagerTrades = vanillaData.villagerTrades,
                    jukeboxes = vanillaData.jukeboxes,
                    bossBars = bossBars,
                    whitelistEnforced = { whitelistEnforced },
                    enchantments = vanillaData.enchantments,
                    dialogs = vanillaData.dialogs,
                    timelines = vanillaData.timelines,
                    progress = playerProgress,
                    advancementTriggers = advancementTriggers,
                    blockTags = vanillaData.blockTags,
                    fluidTags = vanillaData.fluidTags,
                    timelineTags = vanillaData.timelineTags,
                    damageTypeTags = vanillaData.damageTypeTags,
                    entityTypeTags = vanillaData.entityTags,
                    pluginEvents = plugins.events,
                    pluginPackets = plugins.packets,
                    protocolGate = protocolGate,
                    substitutionLabels = substitutionLabels,
                    npcs = npcs,
                    pluginChannel = { channel, uuid, bytes -> plugins.channelMessage(channel, uuid, bytes) },
                    pluginListensOn = { channel -> plugins.listeningOnChannel(channel) },
                    pluginHoldsSidebar = { _ -> plugins.surfaces.anyClaimOn(PluginSurface.SIDEBAR) },
                    pluginSidebar = { uuid ->
                        pluginServices.players.byUuid(uuid)?.let {
                            plugins.surfaces.content(PluginSurface.SIDEBAR, it)?.second
                        }
                    },
                    pluginHoldsBossBar = { plugins.surfaces.anyClaimOn(PluginSurface.BOSS_BAR) },
                    pluginBossBar = { uuid ->
                        pluginServices.players.byUuid(uuid)?.let {
                            plugins.surfaces.content(PluginSurface.BOSS_BAR, it)?.second
                        }
                    },
                    itemTags = vanillaData.itemTags,
                ).run()
            }
        }

        awaitShutdown()
    }

    /**
     * Keeps `main` alive while [shutdown] finishes on another thread: closing the listener
     * unblocks `accept()` and drops out of the loop before the save it started is done, which
     * left `players.db` with rows still in the write-ahead log.
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
        // 1 declared a view/simulation distance of 1 chunk to the client, below what a real
        // client expects and handles well (vanilla's own slider does not even go that low): it
        // looked like "only one chunk renders" even though every chunk actually had real terrain
        // (confirmed directly by reading the saved region files back). 3 keeps startup bounded
        // while staying in the range a real client behaves normally at.
        const val SPAWN_RADIUS_CHUNKS = 3

        /** How often the watchdog looks, and how long a plugin may be inside the server. */
        const val PLUGIN_WATCHDOG_INTERVAL_MILLIS = 250L

        /** Five ticks. Long enough that no honest handler reaches it, short enough to be felt once. */
        const val PLUGIN_STUCK_MILLIS = 250L

        /** parity: `EntityTypes.PLAYER`, whose box is what a mob's reach is measured against. */
        val PLAYER_TYPE: Identifier = Identifier.vanilla("player")

        /** What a cart with nobody in it is pushed by: nothing. */
        val EMPTY_INTENT = doubleArrayOf(0.0, 0.0)

        const val SOUND_TNT_PRIMED = "entity.tnt.primed"

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
