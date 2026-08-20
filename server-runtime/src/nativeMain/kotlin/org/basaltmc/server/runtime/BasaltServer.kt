package org.basaltmc.server.runtime

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.registry.EntityTypes
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
import org.basaltmc.server.runtime.entity.Attribute
import org.basaltmc.server.runtime.entity.Breeding
import org.basaltmc.server.runtime.entity.DamageSource
import org.basaltmc.server.runtime.entity.EntityCombat
import org.basaltmc.server.runtime.entity.EntityNbt
import org.basaltmc.server.runtime.entity.Flyers
import org.basaltmc.server.runtime.entity.Minecarts
import org.basaltmc.server.runtime.entity.MobBrain
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.server.runtime.entity.MobCombat
import org.basaltmc.server.runtime.entity.MobDespawn
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.PrimedTnt
import org.basaltmc.server.runtime.entity.Projectiles
import org.basaltmc.server.runtime.entity.Raid
import org.basaltmc.server.runtime.entity.RaidManager
import org.basaltmc.server.runtime.entity.RaidStatus
import org.basaltmc.server.runtime.entity.RaidWaves
import org.basaltmc.server.runtime.entity.SunBurn
import org.basaltmc.server.runtime.entity.Wardens
import org.basaltmc.server.runtime.entity.ai.LookControl
import org.basaltmc.server.runtime.entity.ai.MobActions
import org.basaltmc.server.runtime.entity.ai.MobContext
import org.basaltmc.server.runtime.entity.ai.MobGoals
import org.basaltmc.server.runtime.entity.ai.MoveControl
import org.basaltmc.server.runtime.entity.ai.Navigation
import org.basaltmc.server.runtime.entity.ai.PlayerView
import org.basaltmc.server.runtime.entity.ai.VillageBlocks
import org.basaltmc.server.runtime.function.FunctionLibrary
import org.basaltmc.server.runtime.placeholder.AnimationPlaceholders
import org.basaltmc.server.runtime.placeholder.PlayerPlaceholders
import org.basaltmc.server.runtime.placeholder.ServerFacts
import org.basaltmc.server.runtime.placeholder.ServerPlaceholders
import org.basaltmc.server.runtime.player.Combat
import org.basaltmc.server.runtime.player.Difficulty
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.registry.VanillaRegistryData
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.scoreboard.Teams
import org.basaltmc.server.runtime.timings.ReportSources
import org.basaltmc.server.runtime.timings.TimingsReport
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.AdvancementTriggers
import org.basaltmc.server.runtime.world.Beds
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.BlockEntityTicks
import org.basaltmc.server.runtime.world.BlockInteractions
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.server.runtime.world.Crafters
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.DetectorRails
import org.basaltmc.server.runtime.world.Dispensers
import org.basaltmc.server.runtime.world.EndPodium
import org.basaltmc.server.runtime.world.EndPortals
import org.basaltmc.server.runtime.world.Explosions
import org.basaltmc.server.runtime.world.FallingBlocks
import org.basaltmc.server.runtime.world.Fires
import org.basaltmc.server.runtime.world.FluidFlow
import org.basaltmc.server.runtime.world.FluidKind
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.Jukeboxes
import org.basaltmc.server.runtime.world.Lecterns
import org.basaltmc.server.runtime.world.Lightning
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.server.runtime.world.NaturalSpawner
import org.basaltmc.server.runtime.world.NeighbourUpdates
import org.basaltmc.server.runtime.world.NetherPortals
import org.basaltmc.server.runtime.world.Pistons
import org.basaltmc.server.runtime.world.PlayerProgress
import org.basaltmc.server.runtime.world.PressurePlates
import org.basaltmc.server.runtime.world.ProjectileHits
import org.basaltmc.server.runtime.world.RandomTicks
import org.basaltmc.server.runtime.world.RealAdvancements
import org.basaltmc.server.runtime.world.RealBiomeSpawns
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
import org.basaltmc.server.runtime.world.RealVillagerTrades
import org.basaltmc.server.runtime.world.RecipeBook
import org.basaltmc.server.runtime.world.Redstone
import org.basaltmc.server.runtime.world.RedstoneComponents
import org.basaltmc.server.runtime.world.RedstoneReactors
import org.basaltmc.server.runtime.world.ScheduledBlocks
import org.basaltmc.server.runtime.world.StructureTemplates
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.Tripwires
import org.basaltmc.server.runtime.world.VanillaWorldGeneratorFactory
import org.basaltmc.server.runtime.world.Vibrations
import org.basaltmc.server.runtime.world.WORLDS_ROOT
import org.basaltmc.server.runtime.world.WorldLight
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
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.BlockEventPacket
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.BossBarColor
import org.basaltmc.wire.protocol.play.BossBarOverlay
import org.basaltmc.wire.protocol.play.BossEventPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.DamageEventPacket
import org.basaltmc.wire.protocol.play.EntityEventPacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.ExplodePacket
import org.basaltmc.wire.protocol.play.ExplosionParticle
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.LevelEventPacket
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.Pose
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SetEntityMotionPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.LangTable
import org.basaltmc.wire.text.TextColor
import org.basaltmc.wire.text.resolvePlainText
import kotlin.concurrent.AtomicInt
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.sqrt
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

    /** What each biome offers the spawner, out of the operator's own jar. */
    private val biomeSpawns = RealBiomeSpawns(serverJar)

    /** What each villager offers, out of the same jar. */
    private val villagerTrades = RealVillagerTrades(serverJar)

    /** The song data a comparator reads off a playing jukebox. */
    private val jukeboxes = Jukeboxes(serverJar)
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
            onSaveInterval = { timings.of("tick.save").time { if (saveControl.enabled) persistEverything() } },
            onUnloadInterval = { timings.of("tick.unload").time { unloadUnreachableChunks() } },
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
    private val randomTicks = RandomTicks()

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

    /** parity: `Raids`, one per world, which this build keeps in memory rather than saved data. */
    private val raidManagers: MutableMap<String, RaidManager> = mutableMapOf()

    /** `/save-off` stops the periodic save; `/save-all` runs one on demand. */
    private val saveControl: SaveControl = SaveControl(::persistEverything)

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

    /** parity: `MinecraftServer.saveEverything`, the whole autosave: every world, then the players. */
    private fun persistEverything() {
        // Emitted before the work, not after: if this method ever stops completing, the last thing
        // in the log is where it got to, instead of nothing at all.
        logger.debug { "Save starting across ${worlds.size} world(s)." }
        for (world in worlds.values) {
            world.saveDirty()
            // Clock, weather and game rules, which no chunk carries: cheap enough to write on every
            // save, and losing an hour of world time to a crash is worse than one small file write.
            runCatching { commandStorage.save() }
                .onFailure { logger.error(it) { "Could not save command storage" } }
            runCatching { world.saveMetadata() }
                .onFailure { logger.error(it) { "Could not save level.nbt for world '${world.name}'" } }
        }
        persistPlayers()
    }

    /** Chunks nobody can reach go, whatever changed in them written on the way out. */
    private fun unloadUnreachableChunks() {
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
            // The tick thread owns every world's state. Claiming it here rather than at startup
            // keeps it true even if the loop ever moves to another thread, and running what other
            // threads left for it *before* the tick is what makes their work part of this tick
            // rather than of an unpredictable one.
            if (!world.owner.isCurrent) world.owner.bindToCurrentThread()
            world.owner.runPending { logger.error(it) { "A task for world '${world.name}' failed" } }
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
            runCatching { tickRandomBlocks(world) }
                .onFailure { logger.error(it) { "Random block tick failed in world '${world.name}'" } }
            runCatching { tickPressurePlates(world) }
                .onFailure { logger.error(it) { "Pressure plate tick failed in world '${world.name}'" } }
            runCatching { tickTripwires(world) }
                .onFailure { logger.error(it) { "Tripwire tick failed in world '${world.name}'" } }
            runCatching { tickFootsteps(world) }
                .onFailure { logger.error(it) { "Footstep tick failed in world '${world.name}'" } }
            runCatching { tickScheduledBlocks(world) }
                .onFailure { logger.error(it) { "Scheduled block tick failed in world '${world.name}'" } }
            runCatching { tickBlockEntities(world) }
                .onFailure { logger.error(it) { "Block entity tick failed in world '${world.name}'" } }
            runCatching { tickFallingBlocks(world) }
                .onFailure { logger.error(it) { "Falling block tick failed in world '${world.name}'" } }
            runCatching { tickEntities(world) }
                .onFailure { logger.error(it) { "Entity tick failed in world '${world.name}'" } }
            runCatching { tickSpawning(world) }
                .onFailure { logger.error(it) { "Mob spawning failed in world '${world.name}'" } }
            runCatching { tickEffects(world) }
                .onFailure { logger.error(it) { "Effect tick failed in world '${world.name}'" } }
            runCatching { tickRaids(world) }
                .onFailure { logger.error(it) { "Raid tick failed in world '${world.name}'" } }
            runCatching { tickSleep(world) }
                .onFailure { logger.error(it) { "Sleep tick failed in world '${world.name}'" } }
        }
        shutdownIfSignalled()
    }

    /** One mob of this type where it was asked for, or null when the type is not one this build has. */
    private fun spawnMob(
        world: LoadedWorld,
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
    ): org.basaltmc.server.runtime.entity.ServerEntity? {
        val protocolType = EntityTypes[type]?.protocolId ?: return null
        val entity =
            org.basaltmc.server.runtime.entity.ServerEntity(
                entityId = playerRegistry.allocateEntityId(),
                uuid = Uuid.random(),
                type = type,
                protocolType = protocolType,
                x = x,
                y = y,
                z = z,
            )
        world.state.spawnEntity(entity)
        return entity
    }

    /** One raid manager per world, made the first time anything asks for one. */
    private fun raidsIn(world: LoadedWorld): RaidManager = raidManagers.getOrPut(world.name) { RaidManager() }

    /**
     * Every raid in one world.
     *
     * parity: `Raids.tick` over `Raid.tick`: a wave every time the last one is dead, fifteen seconds
     * apart, and the raid ends when its waves do rather than when the village stops being one.
     */
    private fun tickRaids(world: LoadedWorld) {
        val manager = raidManagers[world.name] ?: return
        if (manager.all.isEmpty()) return
        val players = playerRegistry.snapshot().filter { it.worldName == world.name }
        val finished = mutableListOf<Raid>()
        for (raid in manager.all.toList()) {
            tickRaid(world, players, raid)
            if (raid.status == RaidStatus.ONGOING || raid.celebrationTicks > 0) continue
            hideRaidBar(players, raid)
            finished.add(raid)
        }
        manager.forget(finished)
    }

    private fun tickRaid(
        world: LoadedWorld,
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        if (raid.status != RaidStatus.ONGOING) {
            raid.celebrationTicks--
            return
        }
        raid.ticksActive++
        if (raid.ticksActive >= Raid.TIMEOUT_TICKS) {
            raid.status = RaidStatus.STOPPED
            raid.celebrationTicks = 0
            return
        }
        var health = 0.0
        raid.raiders.retainAll { id ->
            val raider = world.state.entity(id)
            if (raider == null || raider.removed || raider.health <= 0.0) return@retainAll false
            health += raider.health
            true
        }
        // parity: `updatePlayers`, which is who the bar belongs to and who ends up a hero.
        for (player in players) {
            if (raid.distanceSquaredTo(player.x, player.y, player.z) >= Raid.VALID_RADIUS_SQUARED) continue
            raid.heroes.add(player.entityId)
        }
        if (raid.raiders.isEmpty()) {
            if (!raid.hasMoreWaves) {
                winRaid(players, raid)
                return
            }
            if (raid.cooldown > 0) {
                raid.cooldown--
            } else {
                spawnRaidWave(world, players, raid)
                raid.cooldown = Raid.PRE_RAID_TICKS
            }
        }
        updateRaidBar(players, raid, health)
    }

    /** parity: `spawnGroup`, whose counts are `RaiderType`'s own arrays. */
    private fun spawnRaidWave(
        world: LoadedWorld,
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        val wave = raid.groupsSpawned + 1
        val position = raidSpawnPosition(world, raid)
        if (position == null) {
            logger.debug { "Raid wave $wave found nowhere to spawn around ${raid.centerX}, ${raid.centerZ}" }
            return
        }
        var total = 0.0
        for (type in RaidWaves.types) {
            repeat(RaidWaves.countFor(type, wave)) {
                val raider =
                    spawnMob(
                        world,
                        Identifier.vanilla(type),
                        position[0] + 0.5,
                        position[1].toDouble(),
                        position[2] + 0.5,
                    ) ?: return@repeat
                raider.persistent = true
                raider.raidCenter = intArrayOf(raid.centerX, raid.centerY, raid.centerZ)
                raid.raiders.add(raider.entityId)
                total += raider.health
            }
        }
        raid.groupsSpawned = wave
        raid.totalHealth = total
        // parity: `playSound`, the horn everyone within sixty-four blocks hears.
        sendEffects(
            players.filter { raid.distanceSquaredTo(it.x, it.y, it.z) < RAID_HORN_RANGE_SQUARED },
            position[0],
            position[1],
            position[2],
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_RAID_HORN, SoundSource.NEUTRAL, RAID_HORN_VOLUME, 1f)),
        )
    }

    /** parity: `findRandomSpawnPos`, a ring around the village with ground under it. */
    private fun raidSpawnPosition(
        world: LoadedWorld,
        raid: Raid,
    ): IntArray? {
        repeat(RAID_SPAWN_ATTEMPTS) {
            val angle = Random.nextDouble() * 2.0 * PI
            val distance = RAID_SPAWN_MIN + Random.nextDouble() * (RAID_SPAWN_MAX - RAID_SPAWN_MIN)
            val x = raid.centerX + (cos(angle) * distance).toInt()
            val z = raid.centerZ + (sin(angle) * distance).toInt()
            val ground = groundBelow(world, x, raid.centerY + RAID_SPAWN_LIFT, z) ?: return@repeat
            return intArrayOf(x, ground, z)
        }
        return null
    }

    /** The first solid block under a column, which is where anything spawned outdoors stands. */
    private fun groundBelow(
        world: LoadedWorld,
        x: Int,
        fromY: Int,
        z: Int,
    ): Int? {
        var y = fromY
        var dropped = 0
        while (dropped < RAID_SPAWN_DROP) {
            val below = world.state.getBlockState(x, y - 1, z) ?: return null
            val here = world.state.getBlockState(x, y, z) ?: return null
            if (!BlockShapes.isEmptyShape(below) && BlockShapes.isEmptyShape(here)) return y
            y--
            dropped++
        }
        return null
    }

    /** parity: the victory, which is the hero effect for everybody who was there for it. */
    private fun winRaid(
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        raid.status = RaidStatus.VICTORY
        raid.celebrationTicks = Raid.CELEBRATION_TICKS
        for (player in players) {
            if (player.entityId !in raid.heroes) continue
            player.connection.addEffect(MobEffects.HERO_OF_THE_VILLAGE, Raid.HERO_DURATION_TICKS, raid.omenLevel - 1)
        }
    }

    private fun updateRaidBar(
        players: List<PlayerSession>,
        raid: Raid,
        health: Double,
    ) {
        if (raid.groupsSpawned == 0) return
        val progress =
            if (raid.totalHealth <= 0.0) 0f else (health / raid.totalHealth).toFloat().coerceIn(0f, 1f)
        for (player in players) {
            val inRaid = raid.distanceSquaredTo(player.x, player.y, player.z) < Raid.VALID_RADIUS_SQUARED
            if (!inRaid) {
                if (raid.watchers.remove(player.entityId)) sendRaidBar(player, raid, BossEventPacket.Operation.Remove)
                continue
            }
            if (raid.watchers.add(player.entityId)) {
                sendRaidBar(
                    player,
                    raid,
                    BossEventPacket.Operation.Add(
                        Component.translatable(RAID_BAR_KEY),
                        progress,
                        BossBarColor.RED,
                        BossBarOverlay.NOTCHED_10,
                    ),
                )
            } else {
                sendRaidBar(player, raid, BossEventPacket.Operation.UpdateProgress(progress))
            }
        }
    }

    private fun hideRaidBar(
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        for (player in players) {
            if (!raid.watchers.remove(player.entityId)) continue
            sendRaidBar(player, raid, BossEventPacket.Operation.Remove)
        }
    }

    private fun sendRaidBar(
        player: PlayerSession,
        raid: Raid,
        operation: BossEventPacket.Operation,
    ) {
        player.connection.sendPlayPacket(BossEventPacket.ID) { BossEventPacket(raid.barId, operation).write(it) }
    }

    /**
     * parity: `BadOmenMobEffect`, which swaps itself for a raid omen the moment its carrier walks
     * into a village, and `RaidOmenMobEffect`, which starts the raid where that happened.
     */
    private fun tickOmens(
        world: LoadedWorld,
        player: PlayerSession,
        active: ActiveEffect,
    ) {
        if (player.gamemode == Gamemode.SPECTATOR) return
        if (active.effect == MobEffects.BAD_OMEN) {
            if (world.difficulty == "peaceful") return
            // The effect's own countdown is the clock here: it runs whether or not the day does.
            if (active.remainingTicks % VILLAGE_CHECK_INTERVAL != 0) return
            if (!isVillage(world, player.x, player.y, player.z)) return
            val raid = raidsIn(world).nearby(player.x, player.y, player.z)
            if (raid != null && raid.omenLevel >= Raid.MAX_OMEN_LEVEL) return
            player.effects.remove(MobEffects.BAD_OMEN)
            player.connection.sendEffectRemoved(MobEffects.BAD_OMEN)
            player.raidOmenPosition =
                intArrayOf(
                    floor(player.x).toInt(),
                    floor(player.y).toInt(),
                    floor(player.z).toInt(),
                )
            player.connection.addEffect(MobEffects.RAID_OMEN, RAID_OMEN_TICKS, active.amplifier)
            return
        }
        if (active.effect != MobEffects.RAID_OMEN || active.remainingTicks > 1) return
        val position = player.raidOmenPosition ?: return
        player.raidOmenPosition = null
        val raid =
            raidsIn(world).createOrExtend(position[0], position[1], position[2], active.amplifier, world.difficulty)
        if (raid != null) {
            logger.debug {
                "Raid at ${raid.centerX}, ${raid.centerY}, ${raid.centerZ} in world '${world.name}': " +
                    "${raid.totalGroups} wave(s), omen level ${raid.omenLevel}"
            }
        }
    }

    /**
     * Whether this is a village at all.
     *
     * parity: `ServerLevel.isVillage`, which counts the village points of interest around a spot.
     * There is no point of interest map here, so the beds are counted directly: a place people
     * sleep in is what a raid is aimed at either way.
     */
    private fun isVillage(
        world: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val originX = floor(x).toInt()
        val originY = floor(y).toInt()
        val originZ = floor(z).toInt()
        var beds = 0
        for (dy in -VILLAGE_HEIGHT..VILLAGE_HEIGHT) {
            for (dx in -VILLAGE_RADIUS..VILLAGE_RADIUS) {
                for (dz in -VILLAGE_RADIUS..VILLAGE_RADIUS) {
                    val state = world.state.getBlockState(originX + dx, originY + dy, originZ + dz) ?: continue
                    val block = Blocks.byStateId(state)?.block?.path ?: continue
                    if (block !in VillageBlocks.BEDS) continue
                    beds++
                    if (beds >= VILLAGE_BEDS) return true
                }
            }
        }
        return false
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
            MobEffects.BAD_OMEN, MobEffects.RAID_OMEN ->
                worlds[player.worldName]?.let { tickOmens(it, player, active) }
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
                // parity: `Level.setBlock` from the fluid tick updates the neighbours like any other
                // block change. Without it, water that washed a snow layer away left the grass under
                // it wearing its white top for ever, since nothing ever asked it to look again.
                notifyAround(world, watchers, pos)
            }
        }
    }

    /** The neighbour update any block written by the world itself owes the blocks around it. */
    private fun notifyAround(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        pos: BlockPos,
    ) {
        NeighbourUpdates.afterChange(
            world,
            pos.x,
            pos.y,
            pos.z,
            onReshaped = { stateId, x, y, z ->
                for (player in watchers) {
                    player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                        ClientboundBlockUpdatePacket(BlockPos(x, y, z), stateId).write(it)
                    }
                }
            },
            reshapeSelf = false,
        ) { stateId, x, y, z ->
            val at = BlockPos(x, y, z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(at, Blocks.AIR.stateId).write(it)
                }
            }
            dropWashedAwayBlock(world, at, stateId)
        }
    }

    /**
     * Runs the block ticks that have come due: buttons letting themselves back up, redstone
     * components spending their delay, pistons landing, plates coming back up.
     */
    private fun tickScheduledBlocks(world: LoadedWorld) {
        val due = world.state.dueBlockTicks()
        if (due.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (position in due) {
            // Fire acts on its own clock, and keeps asking for the next turn as it goes.
            if (Fires.isFire(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)) {
                for (change in Fires.tick(
                    world,
                    position[0],
                    position[1],
                    position[2],
                    LegacyRandom(Random.nextLong()),
                )) {
                    val pos = BlockPos(change.x, change.y, change.z)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
                        }
                    }
                }
                continue
            }
            // A block that lost its footing turns into an entity; everything else here is a button.
            if (FallingBlocks.falls(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)) {
                val fell =
                    FallingBlocks.beginFalling(world, position[0], position[1], position[2]) {
                        playerRegistry.allocateEntityId()
                    }
                if (fell != null) {
                    val pos = BlockPos(position[0], position[1], position[2])
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(pos, Blocks.AIR.stateId).write(it)
                        }
                    }
                }
                continue
            }
            val scheduled = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
            ScheduledBlocks.frostedIce(
                world,
                position[0],
                position[1],
                position[2],
                LegacyRandom(Random.nextLong()),
                scheduled,
            )
            ScheduledBlocks.driedGhast(world, position[0], position[1], position[2], scheduled)
            // parity: `RedstoneTorchBlock.tick` and `DiodeBlock.tick`, which is where a component's
            // delay is actually spent. What it changes then settles the dust around it.
            // parity: `RedstoneLampBlock.tick`, the four ticks a lamp waits before going dark, and
            // `TntBlock`'s fuse.
            RedstoneReactors.lampTick(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `BasePressurePlateBlock.tick`, the turn a pressed plate books to come back up.
            if (PressurePlates.isPlate(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)) {
                PressurePlates
                    .update(
                        world,
                        position[0],
                        position[1],
                        position[2],
                        standingOn(world, position[0], position[1], position[2], watchers),
                    )?.let { scheduled += it }
            }
            // parity: `TripWireBlock.tick`, the recheck a pressed wire books for itself.
            if (BlockStateDefinitions.blockOf(world.state.getBlockState(position[0], position[1], position[2]) ?: 0)
                == Tripwires.WIRE
            ) {
                val stillOn =
                    watchers.any {
                        Tripwires.isStandingOn(position[0], position[1], position[2], it.x, it.y, it.z)
                    } ||
                        world.state.items().any {
                            Tripwires.isStandingOn(position[0], position[1], position[2], it.x, it.y, it.z)
                        }
                Tripwires.press(world, position[0], position[1], position[2], stillOn, scheduled)
            }
            // parity: `LecternBlock.tick`, the two ticks a page turn pulses for.
            Lecterns.endPulse(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `DetectorRailBlock.tick`, the recheck a pressed rail books for itself.
            DetectorRails
                .check(world, position[0], position[1], position[2], world.state.entities())
                ?.let { scheduled += it }
            // parity: `SculkSensorBlock.tick`: thirty ticks awake, then ten of cooldown.
            Vibrations.tick(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `LightningRodBlock`'s own scheduled tick, eight after the strike.
            Lightning.releaseRod(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `TargetBlock`'s own scheduled tick, which lets the signal go again.
            ProjectileHits.releaseTarget(world, position[0], position[1], position[2])?.let { scheduled += it }
            // parity: `DispenserBlock.tick`, the four ticks after the signal arrived.
            scheduled += finishPiston(world, position, watchers)
            val dispenserEffects = mutableListOf<BlockEffect>()
            Dispensers.fire(
                world,
                position[0],
                position[1],
                position[2],
                LegacyRandom(Random.nextLong()),
                dispenserEffects,
            ) { playerRegistry.allocateEntityId() }
            if (dispenserEffects.isNotEmpty()) {
                val stateId = world.state.getBlockState(position[0], position[1], position[2]) ?: 0
                sendEffects(watchers, position[0], position[1], position[2], stateId, dispenserEffects)
            }
            // parity: `CrafterBlock.tick`, four ticks after the signal arrived.
            Crafters
                .craft(
                    world,
                    position[0],
                    position[1],
                    position[2],
                    { input -> recipes.resultFor(input) },
                ) { playerRegistry.allocateEntityId() }
                ?.let { scheduled += it }
            RedstoneComponents.tick(world, position[0], position[1], position[2])?.let { moved ->
                scheduled += moved
                val settled = Redstone.settle(world, moved.x, moved.y, moved.z)
                scheduled += settled
                RedstoneComponents.scheduleAround(world, moved.x, moved.y, moved.z)
                // parity: `Level.updateNeighborsAt` after the component wrote its new state, which is
                // what lights the lamp on a repeater's output side: without it only the dust moved,
                // and everything a component feeds directly stayed as it was.
                scheduled += RedstoneReactors.update(world, moved.x, moved.y, moved.z)
                for (change in settled) scheduled += RedstoneReactors.update(world, change.x, change.y, change.z)
            }
            ScheduledBlocks.frogspawn(world, position[0], position[1], position[2], scheduled)
            org.basaltmc.server.runtime.world.Geysers
                .tick(world, position[0], position[1], position[2], scheduled)
            ScheduledBlocks.snifferEgg(
                world,
                position[0],
                position[1],
                position[2],
                LegacyRandom(Random.nextLong()),
                scheduled,
            )
            if (scheduled.isNotEmpty()) {
                for (change in scheduled) {
                    val pos = BlockPos(change.x, change.y, change.z)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
                        }
                    }
                }
                continue
            }
            val released = BlockInteractions.release(world, position[0], position[1], position[2]) ?: continue
            val pos = BlockPos(position[0], position[1], position[2])
            // parity: `ButtonBlock.tick` into `updateNeighbours`: a button coming back up has to
            // take the signal with it, or whatever it powered stays on for good.
            announceRedstone(
                world,
                watchers,
                org.basaltmc.server.runtime.world
                    .BlockChange(pos.x, pos.y, pos.z, released.stateId),
            )
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(pos, released.stateId).write(it)
                }
                player.connection.sendPlayPacket(SoundPacket.ID) {
                    SoundPacket(
                        sound = released.sound,
                        source = SoundSource.BLOCKS,
                        x = pos.x + 0.5,
                        y = pos.y + 0.5,
                        z = pos.z + 0.5,
                        volume = 1f,
                        pitch = 1f,
                        seed = Random.nextLong(),
                    ).write(it)
                }
            }
        }
    }

    /**
     * Presses the plates people are standing on.
     *
     * parity: `BasePressurePlateBlock.entityInside`, which is a collision rather than a tick. Here
     * the players are walked once a tick instead, which reaches the same state on the same tick: a
     * plate is pressed while somebody is on it and books its own turn to come back up.
     */
    private fun tickPressurePlates(world: LoadedWorld) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        if (watchers.isEmpty()) return
        for (player in watchers) {
            if (player.gamemode == Gamemode.SPECTATOR) continue
            val x = floor(player.x).toInt()
            val z = floor(player.z).toInt()
            val y = PressurePlates.blockUnder(player.y)
            val stateId = world.state.getBlockState(x, y, z) ?: continue
            if (!PressurePlates.isPlate(stateId)) continue
            if (!PressurePlates.isStandingOn(x, y, z, player.x, player.y, player.z)) continue
            val change = PressurePlates.update(world, x, y, z, standingOn(world, x, y, z, watchers)) ?: continue
            announceRedstone(world, watchers, change)
        }
    }

    /**
     * The footsteps a sculk sensor hears.
     *
     * parity: `GameEvent.STEP`, which vanilla raises as an entity moves. A player only emits one
     * when they have crossed into another block *and* a sensor is known to be near: the index is
     * what makes that affordable to ask every tick (see [WorldState.hasSculkSensorNear]).
     */
    private fun tickFootsteps(world: LoadedWorld) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        if (watchers.isEmpty()) return
        val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        for (player in watchers) {
            if (player.gamemode == Gamemode.SPECTATOR) continue
            if (!player.onGround) continue
            val block = pack(floor(player.x).toInt(), floor(player.y).toInt(), floor(player.z).toInt())
            if (lastFootstep[player.entityId] == block) continue
            lastFootstep[player.entityId] = block
            // parity: the warden's own `VibrationUser`, which hears a step sixteen blocks away and
            // gets angrier at whoever took it. Sneaking makes no vibration, so it hears nothing.
            if (!player.sneaking) hearFootstep(world, player)
            if (!world.state.hasSculkSensorNear(player.x, player.y, player.z, Vibrations.RANGE)) continue
            Vibrations.emit(world, "step", player.x, player.y, player.z, changes)
        }
        for (change in changes) announceRedstone(world, watchers, change)
    }

    /** parity: `Warden.increaseAngerAt` from a vibration: a step it hears is worth thirty-five. */
    private fun hearFootstep(
        world: LoadedWorld,
        player: PlayerSession,
    ) {
        for (entity in world.state.entities()) {
            if (entity.removed || !Wardens.isWarden(entity.type)) continue
            val dx = entity.x - player.x
            val dy = entity.y - player.y
            val dz = entity.z - player.z
            if (dx * dx + dy * dy + dz * dz > Wardens.HEARING_RANGE * Wardens.HEARING_RANGE) continue
            Wardens.anger(entity, player.entityId, Wardens.HEARD)
        }
    }

    /**
     * Presses the tripwires something is standing in.
     *
     * parity: `TripWireBlock.entityInside`, which is a collision in vanilla and a walk over the
     * players and dropped items here. What it powers is the hook at the end of the string.
     */
    private fun tickTripwires(world: LoadedWorld) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        val crossed = HashSet<Long>()
        for (player in watchers) {
            if (player.gamemode == Gamemode.SPECTATOR) continue
            crossingsOf(world, player.x, player.y, player.z, crossed)
        }
        for (item in world.state.items()) crossingsOf(world, item.x, item.y, item.z, crossed)
        for (packed in crossed) {
            Tripwires.press(
                world,
                unpackX(packed),
                unpackY(packed),
                unpackZ(packed),
                occupied = true,
                changes = changes,
            )
        }
        for (change in changes) announceRedstone(world, watchers, change)
    }

    /** The tripwire block a thing at these coordinates is standing in, if it is standing in one. */
    private fun crossingsOf(
        world: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
        into: MutableSet<Long>,
    ) {
        val blockX = floor(x).toInt()
        val blockZ = floor(z).toInt()
        for (blockY in floor(y - 0.1).toInt()..floor(y).toInt()) {
            val stateId = world.state.getBlockState(blockX, blockY, blockZ) ?: continue
            if (BlockStateDefinitions.blockOf(stateId) != Tripwires.WIRE) continue
            if (!Tripwires.isStandingOn(blockX, blockY, blockZ, x, y, z)) continue
            into += pack(blockX, blockY, blockZ)
        }
    }

    private fun pack(
        x: Int,
        y: Int,
        z: Int,
    ): Long = (x.toLong() and 0x3ffffff shl 38) or (z.toLong() and 0x3ffffff shl 12) or (y.toLong() and 0xfff)

    private fun unpackX(packed: Long): Int = (packed shl 0 shr 38).toInt()

    private fun unpackY(packed: Long): Int = (packed shl 52 shr 52).toInt()

    private fun unpackZ(packed: Long): Int = (packed shl 26 shr 38).toInt()

    /** How many things are on the plate at these coordinates, the way its own sensitivity counts. */
    private fun standingOn(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        watchers: List<PlayerSession>,
    ): Int {
        var count =
            watchers.count {
                it.gamemode != Gamemode.SPECTATOR && PressurePlates.isStandingOn(x, y, z, it.x, it.y, it.z)
            }
        // parity: the stone and blackstone plates are `MOBS`, so a dropped item does not press them.
        val path = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path.orEmpty()
        if (path != "stone_pressure_plate" && path != "polished_blackstone_pressure_plate") {
            count += world.state.items().count { PressurePlates.isStandingOn(x, y, z, it.x, it.y, it.z) }
        }
        return count
    }

    /**
     * One block, its new state, and whatever it asked the clients to hear or see while they are at
     * it: a note, a bell, a door swinging, a dispenser clicking at nothing.
     */
    private fun announceBlock(
        watchers: List<PlayerSession>,
        change: org.basaltmc.server.runtime.world.BlockChange,
    ) {
        val pos = BlockPos(change.x, change.y, change.z)
        for (player in watchers) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
            }
        }
        if (change.effects.isEmpty()) return
        sendEffects(watchers, change.x, change.y, change.z, change.stateId, change.effects)
    }

    /** The effect half on its own, for the blocks that make a noise without changing state. */
    private fun sendEffects(
        watchers: List<PlayerSession>,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        effects: List<BlockEffect>,
    ) {
        val pos = BlockPos(x, y, z)
        for (effect in effects) {
            when (effect) {
                is BlockEffect.Sound -> {
                    val sound =
                        SoundPacket(
                            sound = effect.name,
                            source = effect.source,
                            x = x + 0.5,
                            y = y + 0.5,
                            z = z + 0.5,
                            volume = effect.volume,
                            pitch = effect.pitch,
                            seed = Random.nextLong(),
                        )
                    for (player in watchers) {
                        player.connection.sendPlayPacket(SoundPacket.ID) { sound.write(it) }
                    }
                }

                is BlockEffect.Event -> {
                    val blockId = BlockSupport.blockRegistryIdOf(stateId)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(BlockEventPacket.ID) {
                            BlockEventPacket(pos, effect.b0, effect.b1, blockId).write(it)
                        }
                    }
                }

                is BlockEffect.Effect -> {
                    for (player in watchers) {
                        player.connection.sendPlayPacket(LevelEventPacket.ID) {
                            LevelEventPacket(effect.type, pos, effect.data).write(it)
                        }
                    }
                }
            }
        }
    }

    /** Sends a block that redstone moved to everyone watching, and lets its neighbours react. */
    private fun announceRedstone(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        change: org.basaltmc.server.runtime.world.BlockChange,
    ) {
        announceBlock(watchers, change)
        val settled = Redstone.settle(world, change.x, change.y, change.z)
        for (moved in settled) announceBlock(watchers, moved)
        for (reacted in RedstoneReactors.update(world, change.x, change.y, change.z)) {
            announceBlock(watchers, reacted)
        }
        // parity: every piece of dust that moved tells its own neighbours, so a lamp at the end of
        // the line lights rather than waiting for something to touch it.
        for (moved in settled) {
            for (reacted in RedstoneReactors.update(world, moved.x, moved.y, moved.z)) {
                announceBlock(watchers, reacted)
            }
        }
        RedstoneComponents.scheduleAround(world, change.x, change.y, change.z)
    }

    /**
     * The piston half of a scheduled tick, with the players close enough to be shoved by it.
     *
     * parity: `PistonMovingBlockEntity.moveCollidedEntities`. A player who gets pushed is teleported,
     * because a client only learns it moved when the server says so.
     */
    private fun finishPiston(
        world: LoadedWorld,
        position: IntArray,
        watchers: List<PlayerSession>,
    ): List<org.basaltmc.server.runtime.world.BlockChange> {
        val nearby =
            watchers
                .filter { abs(it.x - position[0]) <= pistonReach && abs(it.z - position[2]) <= pistonReach }
                .map { Pistons.PlayerPosition(it.uuid.toString(), it.x, it.y, it.z) }
        return Pistons.finish(world, position[0], position[1], position[2], nearby) { pushed ->
            val player = watchers.firstOrNull { it.uuid.toString() == pushed.uuid } ?: return@finish
            player.connection.teleportTo(pushed.x, pushed.y, pushed.z, player.yaw, player.pitch)
        }
    }

    /** Which block each player was last heard stepping in, so one step is one vibration. */
    private val lastFootstep = HashMap<Int, Long>()

    /** parity: `AbstractMinecart.getDefaultGravity`, the same 0.04 as anything else that is not alive. */
    private val minecartGravity = 0.04

    /** How far from a piston a player can be and still be in the way of what it pushes. */
    private val pistonReach = 14.0

    /**
     * Fills the world around the players with the mobs their biomes offer.
     *
     * parity: `NaturalSpawner.spawnForChunk`, once per simulated chunk per tick, under a cap per
     * category that scales with how much of the world is being simulated. `doMobSpawning` turns the
     * whole thing off, as it does in vanilla.
     */
    private fun tickSpawning(world: LoadedWorld) {
        if (!world.gameRules[GameRuleRegistry.SPAWN_MOBS]) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        if (watchers.isEmpty()) return
        val chunks = simulatedChunks(world)
        if (chunks.isEmpty()) return

        val alive = world.state.entities().filter { !it.removed }
        val random = LegacyRandom(Random.nextLong())
        // parity: `NaturalSpawner`'s own order, monsters first: the caps are counted per category.
        for (category in listOf(MobCategory.MONSTER, MobCategory.CREATURE, MobCategory.AMBIENT)) {
            val cap = NaturalSpawner.capFor(category, chunks.size)
            val living = alive.count { Mobs.categoryOf(it.type) == category.categoryName }
            if (living >= cap) continue
            // parity: one position per chunk per tick, rather than a sweep of the whole world.
            val chunk = chunks[random.nextInt(chunks.size)]
            val requests =
                NaturalSpawner.spawnForChunk(
                    world,
                    chunk[0],
                    chunk[1],
                    category,
                    entriesFor = { biome -> biomeSpawns.listFor(biome, category.categoryName) },
                    playerDistanceSquared = { x, y, z ->
                        watchers.minOfOrNull { player ->
                            val dx = player.x - x
                            val dy = player.y - y
                            val dz = player.z - z
                            dx * dx + dy * dy + dz * dz
                        }
                    },
                    random = random,
                )
            for (request in requests) {
                val protocolType = EntityTypes[request.type]?.protocolId ?: continue
                world.state.spawnEntity(
                    org.basaltmc.server.runtime.entity.ServerEntity(
                        entityId = playerRegistry.allocateEntityId(),
                        uuid = Uuid.random(),
                        type = request.type,
                        protocolType = protocolType,
                        x = request.x,
                        y = request.y,
                        z = request.z,
                        yaw = request.yaw,
                    ),
                )
            }
        }
    }

    /**
     * Ticks the entities that are not players, items or falling blocks: today that is primed TNT.
     *
     * parity: `ServerLevel.tickNonPassenger`, one turn per entity per tick, in the order they were
     * spawned. What each one does is its own class's `tick`.
     */
    private fun tickEntities(world: LoadedWorld) {
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
                sendEffects(
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
            val type = runCatching { Identifier.parse(tag.getString("id")) }.getOrNull() ?: continue
            val protocol = EntityTypes[type]?.protocolId ?: continue
            val restored = EntityNbt.decode(tag, playerRegistry.allocateEntityId(), protocol) ?: continue
            world.state.spawnEntity(restored)
        }
        // parity: `Entity.die`, which drops the entity's own loot table where it stood. Whatever was
        // killed since the last tick is dealt with here, once, before anything else moves.
        for (dead in world.state.entities().filter { it.killed }) {
            dead.killed = false
            dropDeathLoot(world, dead)
            // parity: `GameEvent.ENTITY_DIE`, the loudest thing a sculk sensor hears.
            val heard = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
            Vibrations.emit(world, "entity_die", dead.x, dead.y, dead.z, heard)
            for (change in heard) announceRedstone(world, watchers, change)
        }
        // One snapshot for the whole tick: `entities()` takes the world's lock and copies the list,
        // and asking for it per entity turned a pen of cows into a lock convoy.
        val living = world.state.entities().filter { !it.removed }
        // parity: `TargetingConditions.forCombat`, which skips anybody a mob cannot fight.
        val views =
            watchers.map { player ->
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
            if (entity.type == PrimedTnt.TYPE) {
                if (!PrimedTnt.tick(world.state, entity)) continue
                entity.removed = true
                blowUp(
                    world,
                    watchers,
                    entity.x,
                    entity.y + PrimedTnt.EXPLOSION_OFFSET,
                    entity.z,
                    PrimedTnt.EXPLOSION_POWER,
                )
                // parity: `GameEvent.EXPLODE`, which is fifteen: the top of the scale.
                val heard = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
                Vibrations.emit(world, "explode", entity.x, entity.y, entity.z, heard)
                for (change in heard) announceRedstone(world, watchers, change)
                continue
            }
            val projectile = Projectiles.kindOf(entity.type)
            if (projectile != null) {
                tickProjectile(world, watchers, living, entity, projectile)
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
                    if (!touchesCart(entity, player)) continue
                    Minecarts.pushedBy(entity, player.x, player.z)
                }
                val intent =
                    rider?.let {
                        Minecarts.intentOf(it.inputForward, it.inputBack, it.inputLeft, it.inputRight, it.yaw)
                    } ?: EMPTY_INTENT
                if (!Minecarts.moveAlongTrack(world.state, entity, intent[0], intent[1], rider != null)) {
                    Minecarts.comeOffTrack(world.state, entity, minecartGravity)
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
                pressed?.let { announceRedstone(world, watchers, it) }
                continue
            }
            // parity: `Mob.aiStep`, which is what makes an animal wander rather than stand there.
            if (Mobs.isMob(entity.type)) {
                entity.idleTicks++
                entity.tickCount++
                EntityCombat.tick(entity)
                if (Wardens.isWarden(entity.type)) {
                    Wardens.tick(entity)
                    if (entity.touchCooldown > 0) entity.touchCooldown--
                }
                if (entity.panicTicks > 0) entity.panicTicks--
                tickFire(world, watchers, entity, random)
                tickMobAi(world, entity, views, living, random)
                // parity: `Animal.aiStep` and `BreedGoal`, which is where a calf comes from.
                val partner = Breeding.tick(entity, living)
                if (partner != null) breed(world, entity, partner)
                // parity: `Mob.checkDespawn`, which vanilla runs on the same tick as the AI.
                val category = MobCategory.byName(Mobs.categoryOf(entity.type)) ?: MobCategory.MISC
                val nearest = watchers.minOfOrNull { MobBrain.distanceTo(entity, it.x, it.y, it.z) }
                if (MobDespawn.shouldDespawn(entity, category, nearest, random)) entity.removed = true
            }
        }
    }

    /**
     * What the goals are allowed to make the world do: swing, hit, breed, eat.
     *
     * The goals cannot reach the server themselves, so this is the whole of their arm's length.
     */
    private val mobActions =
        object : MobActions {
            override fun swing(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
                    player.connection.sendPlayPacket(AnimatePacket.ID) {
                        AnimatePacket(mob.entityId, AnimatePacket.SWING_MAIN_HAND).write(it)
                    }
                }
            }

            override fun hurtPlayer(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                playerId: Int,
                damage: Float,
            ) {
                val player = playerRegistry.snapshot().firstOrNull { it.entityId == playerId } ?: return
                val world = worlds[player.worldName] ?: return
                // parity: `Player.hurtServer`, where the difficulty decides how much of it lands.
                player.connection.hurt(
                    Combat.afterDifficulty(damage, world.difficulty),
                    DEATH_MOB,
                    DamageTypes.MOB_ATTACK,
                    DamageSource(mob.entityId, mob.x, mob.z),
                )
                // parity: `OwnerHurtByTargetGoal`: a pet goes for whatever hit the person it follows.
                for (pet in world.state.entities()) {
                    if (pet.owner == player.uuid.toString()) pet.ownerHurtBy = mob.entityId
                }
            }

            override fun hurtMob(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                target: org.basaltmc.server.runtime.entity.ServerEntity,
                damage: Float,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(target.entityId) != null } ?: return
                val result = EntityCombat.hurt(target, damage, mob.x, mob.z)
                if (!result.landed) return
                target.hurtBy = mob.entityId
                target.panicTicks = PANIC_TICKS
                val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
                announceEntityHit(watchers, target, DamageTypes.MOB_ATTACK, mob.entityId, result.died)
            }

            override fun breed(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                partner: org.basaltmc.server.runtime.entity.ServerEntity,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                breed(world, mob, partner)
            }

            override fun eatGrass(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                eatGrassUnder(world, mob)
            }

            override fun setCharging(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                charging: Boolean,
            ) {
                if (mob.charging == charging) return
                mob.charging = charging
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val index = if (mob.type.path == "blaze") BLAZE_CHARGED_INDEX else GHAST_CHARGING_INDEX
                val entry =
                    if (mob.type.path == "blaze") {
                        SetEntityDataPacket.Entry.Bytes(index, (if (charging) BLAZE_CHARGED_FLAG else 0).toByte())
                    } else {
                        SetEntityDataPacket.Entry.Booleans(index, charging)
                    }
                for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
                    player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                        SetEntityDataPacket(mob.entityId, listOf(entry)).write(it)
                    }
                }
            }

            override fun shootFireball(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                targetX: Double,
                targetY: Double,
                targetZ: Double,
                large: Boolean,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val kind = if (large) Projectiles.FIREBALL else Projectiles.SMALL_FIREBALL
                val protocolType = EntityTypes[kind.type]?.protocolId ?: return
                val fromY = mob.y + mob.height * FIREBALL_EYE_SHARE
                val ball =
                    Projectiles.spawn(
                        world = world.state,
                        kind = kind,
                        entityId = playerRegistry.allocateEntityId(),
                        protocolType = protocolType,
                        x = mob.x,
                        y = fromY,
                        z = mob.z,
                        dirX = targetX - mob.x,
                        dirY = targetY - fromY,
                        dirZ = targetZ - mob.z,
                        power = FIREBALL_POWER,
                        uncertainty = if (large) 0f else SMALL_FIREBALL_SPREAD,
                        random = LegacyRandom(Random.nextLong()),
                    )
                ball.ownerId = mob.entityId
                ball.explosionPower = if (large) GHAST_EXPLOSION_POWER else 0f
                sendEffects(
                    playerRegistry.snapshot().filter { it.worldName == world.name },
                    floor(mob.x).toInt(),
                    floor(mob.y).toInt(),
                    floor(mob.z).toInt(),
                    Blocks.AIR.stateId,
                    listOf(BlockEffect.Sound(SOUND_FIREBALL_SHOOT, SoundSource.HOSTILE, 1f, 1f)),
                )
            }

            override fun setBeamTarget(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                targetId: Int?,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
                    player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                        SetEntityDataPacket(
                            mob.entityId,
                            listOf(SetEntityDataPacket.Entry.Ints(GUARDIAN_BEAM_INDEX, targetId ?: 0)),
                        ).write(it)
                    }
                }
            }

            override fun setSleeping(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                sleeping: Boolean,
            ) {
                if (mob.sleeping == sleeping) return
                mob.sleeping = sleeping
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val pose = if (sleeping) Pose.SLEEPING else Pose.STANDING
                for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
                    player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                        SetEntityDataPacket(
                            mob.entityId,
                            listOf(SetEntityDataPacket.Entry.Poses(SetEntityDataPacket.POSE_INDEX, pose)),
                        ).write(it)
                    }
                }
            }

            override fun setCasting(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                casting: Boolean,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val spell = if (casting) SPELL_CASTING else SPELL_NONE
                for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
                    player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                        SetEntityDataPacket(
                            mob.entityId,
                            listOf(SetEntityDataPacket.Entry.Bytes(SPELL_INDEX, spell.toByte())),
                        ).write(it)
                    }
                }
            }

            override fun throwPotion(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                targetX: Double,
                targetY: Double,
                targetZ: Double,
                potion: String,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val protocolType = EntityTypes[Projectiles.SPLASH_POTION.type]?.protocolId ?: return
                val fromY = mob.y + mob.height * POTION_THROW_SHARE
                val dx = targetX - mob.x
                val dz = targetZ - mob.z
                val flat = sqrt(dx * dx + dz * dz)
                val bottle =
                    Projectiles.spawn(
                        world = world.state,
                        kind = Projectiles.SPLASH_POTION,
                        entityId = playerRegistry.allocateEntityId(),
                        protocolType = protocolType,
                        x = mob.x,
                        y = fromY,
                        z = mob.z,
                        dirX = dx,
                        // parity: `performRangedAttack`'s own arc, a fifth of the distance across.
                        dirY = targetY - fromY + flat * POTION_ARC,
                        dirZ = dz,
                        power = POTION_POWER,
                        uncertainty = POTION_SPREAD,
                        random = LegacyRandom(Random.nextLong()),
                    )
                bottle.ownerId = mob.entityId
                bottle.potionEffect = potion
                sendEffects(
                    playerRegistry.snapshot().filter { it.worldName == world.name },
                    floor(mob.x).toInt(),
                    floor(mob.y).toInt(),
                    floor(mob.z).toInt(),
                    Blocks.AIR.stateId,
                    listOf(BlockEffect.Sound(SOUND_WITCH_THROW, SoundSource.HOSTILE, 1f, 1f)),
                )
            }

            override fun castFangs(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                targetX: Double,
                targetY: Double,
                targetZ: Double,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
                // parity: `Evoker.EvokerAttackSpellGoal`, two lines of fangs walking towards you.
                val angle = atan2(targetZ - mob.z, targetX - mob.x)
                for (step in 0 until FANG_STEPS) {
                    val reach = FANG_START + step * FANG_SPACING
                    val x = mob.x + cos(angle) * reach
                    val z = mob.z + sin(angle) * reach
                    for (player in watchers) {
                        val dx = player.x - x
                        val dz = player.z - z
                        if (dx * dx + dz * dz > FANG_HIT_RANGE) continue
                        player.connection.hurt(
                            FANG_DAMAGE,
                            DEATH_MOB,
                            DamageTypes.MOB_ATTACK,
                            DamageSource(mob.entityId, mob.x, mob.z),
                        )
                    }
                    for (watcher in watchers) {
                        watcher.connection.sendPlayPacket(LevelParticlesPacket.ID) {
                            LevelParticlesPacket(
                                particleId("crit"),
                                x,
                                targetY + FANG_PARTICLE_LIFT,
                                z,
                                count = FANG_PARTICLES,
                            ).write(it)
                        }
                    }
                }
                sendEffects(
                    watchers,
                    floor(mob.x).toInt(),
                    floor(mob.y).toInt(),
                    floor(mob.z).toInt(),
                    Blocks.AIR.stateId,
                    listOf(BlockEffect.Sound(SOUND_FANGS, SoundSource.HOSTILE, 1f, 1f)),
                )
            }

            override fun summonVexes(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                count: Int,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                repeat(count) {
                    val vex =
                        spawnMob(
                            world,
                            VEX_TYPE,
                            mob.x + Random.nextDouble() * 2.0 - 1.0,
                            mob.y + 1.0,
                            mob.z + Random.nextDouble() * 2.0 - 1.0,
                        )
                    vex?.targetId = mob.targetId
                }
            }

            override fun applyEffect(
                playerId: Int,
                effect: String,
                durationTicks: Int,
                amplifier: Int,
            ) {
                val player = playerRegistry.snapshot().firstOrNull { it.entityId == playerId } ?: return
                player.connection.addEffect(Identifier.vanilla(effect), durationTicks, amplifier)
            }

            override fun teleported(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
                sendEffects(
                    watchers,
                    floor(mob.x).toInt(),
                    floor(mob.y).toInt(),
                    floor(mob.z).toInt(),
                    Blocks.AIR.stateId,
                    listOf(BlockEffect.Sound(SOUND_ENDERMAN_TELEPORT, SoundSource.HOSTILE, 1f, 1f)),
                )
                // The client has to be told where it went: it is a jump, not a walk.
                for (player in watchers) {
                    player.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                        EntityTeleportPacket(mob.entityId, mob.x, mob.y, mob.z, mob.yaw, mob.pitch, true).write(it)
                    }
                }
            }

            override fun primeCreeper(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
                sendEffects(
                    watchers,
                    floor(mob.x).toInt(),
                    floor(mob.y).toInt(),
                    floor(mob.z).toInt(),
                    Blocks.AIR.stateId,
                    listOf(BlockEffect.Sound(SOUND_CREEPER_PRIMED, SoundSource.HOSTILE, 1f, 0.5f)),
                )
            }

            override fun explodeCreeper(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
                mob.removed = true
                blowUp(world, watchers, mob.x, mob.y, mob.z, CREEPER_EXPLOSION_POWER)
            }

            override fun shootArrow(
                mob: org.basaltmc.server.runtime.entity.ServerEntity,
                targetX: Double,
                targetY: Double,
                targetZ: Double,
                power: Float,
            ) {
                val world = worlds.values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
                val protocolType = EntityTypes[Projectiles.ARROW.type]?.protocolId ?: return
                val fromY = mob.y + mob.height * ARROW_EYE_SHARE
                val dx = targetX - mob.x
                val dy = targetY + ARROW_AIM_SHARE - fromY
                val dz = targetZ - mob.z
                val flat = sqrt(dx * dx + dz * dz)
                val arrow =
                    Projectiles.spawn(
                        world = world.state,
                        kind = Projectiles.ARROW,
                        entityId = playerRegistry.allocateEntityId(),
                        protocolType = protocolType,
                        x = mob.x,
                        y = fromY,
                        z = mob.z,
                        dirX = dx,
                        // parity: the arc a skeleton adds, a fifth of the distance to the target.
                        dirY = dy + flat * ARROW_ARC,
                        dirZ = dz,
                        power = ARROW_POWER,
                        uncertainty = arrowSpread(world),
                        random = LegacyRandom(Random.nextLong()),
                    )
                arrow.ownerId = mob.entityId
                sendEffects(
                    playerRegistry.snapshot().filter { it.worldName == world.name },
                    floor(mob.x).toInt(),
                    floor(mob.y).toInt(),
                    floor(mob.z).toInt(),
                    Blocks.AIR.stateId,
                    listOf(BlockEffect.Sound(SOUND_SKELETON_SHOOT, SoundSource.HOSTILE, 1f, 1f)),
                )
            }
        }

    /** parity: `14 - difficulty * 4`, the spread a skeleton's shot has. */
    private fun arrowSpread(world: LoadedWorld): Float =
        when (world.difficulty) {
            "peaceful" -> 14f
            "easy" -> 10f
            "normal" -> 6f
            else -> 2f
        }

    /**
     * parity: `EatBlockGoal`: the grass in front becomes air and the block below becomes dirt, and
     * a lamb that eats grows a little.
     */
    private fun eatGrassUnder(
        world: LoadedWorld,
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        val x = floor(mob.x).toInt()
        val y = floor(mob.y).toInt()
        val z = floor(mob.z).toInt()
        val here = world.state.getBlockState(x, y, z)
        val shortGrass = Blocks.REGISTRY.idOf(Identifier.vanilla("short_grass"))
        if (here != null && here == shortGrass) {
            if (world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) {
                announceBlock(
                    watchers,
                    org.basaltmc.server.runtime.world
                        .BlockChange(x, y, z, Blocks.AIR.stateId),
                )
            }
            return
        }
        val below = world.state.getBlockState(x, y - 1, z) ?: return
        if (below != Blocks.GRASS_BLOCK.stateId) return
        val dirt = Blocks.REGISTRY.idOf(Identifier.vanilla("dirt")) ?: return
        if (!world.state.setBlockState(x, y - 1, z, dirt)) return
        announceBlock(
            watchers,
            org.basaltmc.server.runtime.world
                .BlockChange(x, y - 1, z, dirt),
        )
    }

    /**
     * The undead burning in the sun, and what burning costs anything that is on fire.
     *
     * parity: `Mob.burnUndead` and `Entity.baseTick`: eight seconds of fire, a point of damage every
     * twenty ticks of it, and the flames the client draws off the shared flags.
     */
    private fun tickFire(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        random: LegacyRandom,
    ) {
        val wasBurning = mob.fireTicks > 0
        val blockX = floor(mob.x).toInt()
        val blockY = floor(mob.y).toInt()
        val blockZ = floor(mob.z).toInt()
        if (SunBurn.burns(mob.type)) {
            val day = !Beds.isNight(world.clock.totalTicks)
            val light = WorldLight.skyLight(world, blockX, blockY, blockZ)
            val openSky = WorldLight.skyLight(world, blockX, blockY + 1, blockZ) >= SKY_LIGHT_MAX
            val inWater = Fluids.of(world.state.getBlockState(blockX, blockY, blockZ) ?: 0) == Submersion.WATER
            if (SunBurn.shouldBurn(mob.type, day, light, openSky, inWater, random)) {
                mob.fireTicks = SunBurn.FIRE_TICKS
            }
        }
        if (mob.fireTicks > 0) {
            mob.fireTicks--
            if (mob.fireTicks % SunBurn.DAMAGE_INTERVAL == 0) {
                val result = EntityCombat.hurt(mob, SunBurn.FIRE_DAMAGE)
                if (result.landed) announceEntityHit(watchers, mob, DamageTypes.ON_FIRE, -1, result.died)
            }
        }
        val burning = mob.fireTicks > 0
        if (burning == wasBurning) return
        val flags = if (burning) SetEntityDataPacket.FLAG_ON_FIRE else 0
        for (player in watchers) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                SetEntityDataPacket(
                    mob.entityId,
                    listOf(SetEntityDataPacket.Entry.Bytes(SetEntityDataPacket.SHARED_FLAGS_INDEX, flags.toByte())),
                ).write(it)
            }
        }
    }

    /**
     * One mob's turn to decide and then move.
     *
     * parity: `Mob.serverAiStep`: the selectors decide on every other tick (staggered by the entity
     * id, so half the mobs decide on each), and then the navigation, the move control and the look
     * control run every tick.
     */
    private fun tickMobAi(
        world: LoadedWorld,
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        views: List<PlayerView>,
        living: List<org.basaltmc.server.runtime.entity.ServerEntity>,
        random: LegacyRandom,
    ) {
        val selector = mob.goals ?: MobGoals.forType(mob.type)?.also { mob.goals = it } ?: return
        val context =
            MobContext(
                world = world.state,
                mob = mob,
                random = random,
                // The tick's own snapshot, taken once for the whole world: asking for it per mob
                // turned a pen of cows into a copy of the entity list per cow per tick.
                entities = living,
                players = views,
                actions = mobActions,
                light = lightAt(world, MobCombat.blockPosition(mob)),
                night = Beds.isNight(world.clock.totalTicks),
                dayTime = world.clock.totalTicks,
                raidNearby = raidManagers[world.name]?.nearby(mob.x, mob.y, mob.z) != null,
                speed = mob.attributeWithModifiers(Attribute.MOVEMENT_SPEED),
            )
        // parity: `Mob.serverAiStep`, which decides on every other tick and staggers the mobs by
        // their own id so they do not all think on the same one.
        if ((mob.tickCount + mob.entityId) % 2 != 0) {
            selector.tickRunning(context, all = false)
        } else {
            selector.tick(context)
        }
        // parity: `Mob.travel`'s flying branch: what flies is never walked along a path, and the
        // goals move it themselves, so all that is left here is the drift.
        if (Flyers.flies(mob.type)) {
            org.basaltmc.server.runtime.entity.FlyingMovement
                .tick(world.state, mob)
            LookControl.tick(mob)
            return
        }
        Navigation.tick(world.state, mob)
        MoveControl.tick(world.state, mob, context.speed)
        LookControl.tick(mob)
    }

    /** Whether somebody is stood close enough to a cart to shove it. parity: two boxes touching. */
    private fun touchesCart(
        cart: org.basaltmc.server.runtime.entity.ServerEntity,
        player: PlayerSession,
    ): Boolean {
        val reach = (cart.width + EntityTypeShapes.widthOf(PLAYER_TYPE)) / 2.0
        if (abs(player.x - cart.x) > reach || abs(player.z - cart.z) > reach) return false
        return player.y <= cart.y + cart.height && player.y + EntityTypeShapes.heightOf(PLAYER_TYPE) >= cart.y
    }

    /** The light a mob is standing in, which is all the spiders need to know. */
    private fun lightAt(
        world: LoadedWorld,
        position: IntArray,
    ): Int {
        val block = WorldLight.blockLight(world, position[0], position[1], position[2])
        val sky = WorldLight.skyLight(world, position[0], position[1], position[2])
        // parity: `Entity.getLightLevelDependentMagicValue`, whose sky half is darkened by the time
        // of day. Night is taken as darkness rather than tracking the sky brightness itself.
        val daylight = if (Beds.isNight(world.clock.totalTicks)) 0 else sky
        return maxOf(block, daylight)
    }

    /** parity: `Entity.die` over `LootTable`, the drops a mob leaves where it fell. */
    private fun dropDeathLoot(
        world: LoadedWorld,
        entity: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val table = lootTables.forEntity(entity.type) ?: return
        for (stack in table.generate(LegacyRandom(Random.nextLong()))) {
            if (Items.REGISTRY.byIdentifier(stack.item) == null) continue
            world.state.spawnItem(
                WorldState.DroppedItem(
                    entityId = playerRegistry.allocateEntityId(),
                    uuid = Uuid.random(),
                    item = stack.item,
                    count = stack.count,
                    x = entity.x,
                    y = entity.y + entity.height / 2,
                    z = entity.z,
                ),
            )
        }
    }

    /** parity: `Animal.spawnChildFromBreeding`: the calf appears where its parent stands. */
    private fun breed(
        world: LoadedWorld,
        animal: org.basaltmc.server.runtime.entity.ServerEntity,
        partner: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val protocolType = EntityTypes[animal.type]?.protocolId ?: return
        val baby =
            org.basaltmc.server.runtime.entity.ServerEntity(
                entityId = playerRegistry.allocateEntityId(),
                uuid = Uuid.random(),
                type = animal.type,
                protocolType = protocolType,
                x = animal.x,
                y = animal.y,
                z = animal.z,
            )
        baby.age = Breeding.BABY_AGE
        world.state.spawnEntity(baby)
        Breeding.afterBreeding(animal, partner)
    }

    /**
     * One tick of something in flight, and whatever it hits on the way.
     *
     * parity: `Projectile.hitTargetOrDeflectSelf`: what sticks stays where it landed, what does not
     * is gone the moment it lands. What the block itself does about it is `onProjectileHit`.
     */
    private fun tickProjectile(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        living: List<org.basaltmc.server.runtime.entity.ServerEntity>,
        entity: org.basaltmc.server.runtime.entity.ServerEntity,
        kind: Projectiles.Kind,
    ) {
        val fromX = entity.x
        val fromY = entity.y
        val fromZ = entity.z
        val hit = Projectiles.tick(world.state, entity, kind)
        // parity: `AbstractHurtingProjectile.onHit`: a fireball goes off where it lands, whether
        // that is a block or somebody's face.
        if (kind == Projectiles.FIREBALL || kind == Projectiles.SMALL_FIREBALL) {
            val target = targetAlong(watchers, living, entity, fromX, fromY, fromZ)
            if (hit == null && target == null) return
            entity.removed = true
            if (kind == Projectiles.FIREBALL) {
                blowUp(world, watchers, entity.x, entity.y, entity.z, entity.explosionPower)
                return
            }
            // parity: a small fireball burns whoever it hits for five seconds and takes five off.
            val (mob, player) = target ?: return
            if (mob != null) {
                mob.fireTicks = SMALL_FIREBALL_FIRE_TICKS
                val result = EntityCombat.hurt(mob, SMALL_FIREBALL_DAMAGE, fromX, fromZ)
                if (result.landed) announceEntityHit(watchers, mob, DamageTypes.ON_FIRE, entity.entityId, result.died)
            }
            player?.connection?.hurt(
                SMALL_FIREBALL_DAMAGE,
                DEATH_ON_FIRE,
                DamageTypes.ON_FIRE,
                DamageSource(entity.entityId, fromX, fromZ),
            )
            return
        }
        if (kind == Projectiles.ENDER_PEARL) {
            val target = targetAlong(watchers, living, entity, fromX, fromY, fromZ)
            if (hit != null || target != null) landEnderPearl(world, watchers, entity)
            return
        }
        if (kind == Projectiles.SPLASH_POTION) {
            val target = targetAlong(watchers, living, entity, fromX, fromY, fromZ)
            if (hit == null && target == null) return
            entity.removed = true
            splashPotion(watchers, entity)
            return
        }
        if (kind.sticks && hitEntityAlong(watchers, living, entity, fromX, fromY, fromZ)) return
        if (hit == null) return
        ProjectileHits.onHit(world, hit, byArrow = kind.sticks)?.let { change ->
            announceRedstone(world, watchers, change)
        }
        if (!kind.sticks) entity.removed = true
    }

    /**
     * What a bottle leaves where it broke.
     *
     * parity: `ThrownSplashPotion.applySplash`: four blocks of reach, and the further away you are
     * the less of the effect you get, down to nothing at the edge.
     */
    private fun splashPotion(
        watchers: List<PlayerSession>,
        bottle: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val effect = bottle.potionEffect ?: return
        for (player in watchers) {
            val dx = player.x - bottle.x
            val dy = player.y - bottle.y
            val dz = player.z - bottle.z
            val distanceSquared = dx * dx + dy * dy + dz * dz
            if (distanceSquared > SPLASH_RANGE * SPLASH_RANGE) continue
            val share = 1.0 - sqrt(distanceSquared) / SPLASH_RANGE
            val ticks = (SPLASH_DURATION * share).toInt()
            if (ticks <= 0) continue
            player.connection.addEffect(Identifier.vanilla(effect), ticks, 0)
        }
        sendEffects(
            watchers,
            floor(bottle.x).toInt(),
            floor(bottle.y).toInt(),
            floor(bottle.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_SPLASH, SoundSource.NEUTRAL, 1f, 1f)),
        )
    }

    /**
     * Where a thrown pearl puts whoever threw it.
     *
     * parity: `ThrownEnderpearl.onHit`: the owner lands where the pearl stopped, takes five points
     * of fall damage, and both ends of the trip make the teleport sound.
     */
    private fun landEnderPearl(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        pearl: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        pearl.removed = true
        val owner = watchers.firstOrNull { it.entityId == pearl.ownerId } ?: return
        if (owner.dead) return
        sendEffects(
            watchers,
            floor(pearl.x).toInt(),
            floor(pearl.y).toInt(),
            floor(pearl.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_TELEPORT, SoundSource.PLAYERS, 1f, 1f)),
        )
        owner.connection.teleportTo(pearl.x, pearl.y, pearl.z, owner.yaw, owner.pitch)
        owner.connection.hurt(ENDER_PEARL_DAMAGE, DEATH_FALL, DamageTypes.ENDER_PEARL)
    }

    /** The nearest thing a projectile passed through this tick, as a mob or as a player. */
    private fun targetAlong(
        watchers: List<PlayerSession>,
        living: List<org.basaltmc.server.runtime.entity.ServerEntity>,
        projectile: org.basaltmc.server.runtime.entity.ServerEntity,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
    ): Pair<org.basaltmc.server.runtime.entity.ServerEntity?, PlayerSession?>? {
        var nearest = Double.MAX_VALUE
        var mob: org.basaltmc.server.runtime.entity.ServerEntity? = null
        var player: PlayerSession? = null
        for (candidate in living) {
            if (candidate.entityId == projectile.entityId || candidate.entityId == projectile.ownerId) continue
            if (!EntityAttributes.isLiving(candidate.type)) continue
            val at =
                clip(
                    projectile,
                    fromX,
                    fromY,
                    fromZ,
                    candidate.x,
                    candidate.y,
                    candidate.z,
                    candidate.width,
                    candidate.height,
                )
            if (at == null || at >= nearest) continue
            nearest = at
            mob = candidate
            player = null
        }
        for (candidate in watchers) {
            if (candidate.entityId == projectile.ownerId || candidate.dead) continue
            if (candidate.gamemode == Gamemode.CREATIVE || candidate.gamemode == Gamemode.SPECTATOR) continue
            val at =
                clip(
                    projectile,
                    fromX,
                    fromY,
                    fromZ,
                    candidate.x,
                    candidate.y,
                    candidate.z,
                    EntityTypeShapes.widthOf(PLAYER_TYPE),
                    EntityTypeShapes.heightOf(PLAYER_TYPE),
                )
            if (at == null || at >= nearest) continue
            nearest = at
            mob = null
            player = candidate
        }
        if (mob == null && player == null) return null
        return mob to player
    }

    /**
     * Whatever the arrow passed through on its way, hurt.
     *
     * parity: `AbstractArrow.onHitEntity`: the damage is the speed times the arrow's own two
     * points, rounded up, and a fully drawn bow adds a roll on top of it.
     */
    private fun hitEntityAlong(
        watchers: List<PlayerSession>,
        living: List<org.basaltmc.server.runtime.entity.ServerEntity>,
        arrow: org.basaltmc.server.runtime.entity.ServerEntity,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
    ): Boolean {
        val (mob, player) = targetAlong(watchers, living, arrow, fromX, fromY, fromZ) ?: return false
        val speed =
            sqrt(
                arrow.velocityX * arrow.velocityX +
                    arrow.velocityY * arrow.velocityY +
                    arrow.velocityZ * arrow.velocityZ,
            )
        var damage = ceil(speed * ARROW_BASE_DAMAGE).toInt()
        if (arrow.critical) damage += Random.nextInt(damage / 2 + 2)

        if (mob != null) {
            val result = EntityCombat.hurt(mob, damage.toFloat(), fromX, fromZ)
            if (!result.landed) {
                // parity: an arrow that found somebody still inside their hurt cooldown bounces off.
                arrow.velocityX *= ARROW_BOUNCE
                arrow.velocityY *= ARROW_BOUNCE
                arrow.velocityZ *= ARROW_BOUNCE
                return true
            }
            arrow.removed = true
            arrow.ownerId?.let { mob.hurtBy = it }
            mob.panicTicks = PANIC_TICKS
            announceEntityHit(watchers, mob, DamageTypes.ARROW, arrow.entityId, result.died)
            return true
        }
        arrow.removed = true
        player?.connection?.hurt(
            damage.toFloat(),
            DEATH_ARROW,
            DamageTypes.ARROW,
            DamageSource(arrow.entityId, fromX, fromZ),
        )
        return true
    }

    /** Where along its flight the arrow met this box, or null when it did not. */
    private fun clip(
        arrow: org.basaltmc.server.runtime.entity.ServerEntity,
        fromX: Double,
        fromY: Double,
        fromZ: Double,
        x: Double,
        y: Double,
        z: Double,
        width: Double,
        height: Double,
    ): Double? =
        Projectiles.clipBox(
            fromX,
            fromY,
            fromZ,
            arrow.x,
            arrow.y,
            arrow.z,
            x - width / 2.0,
            y,
            z - width / 2.0,
            x + width / 2.0,
            y + height,
            z + width / 2.0,
        )

    /** The flash, the push and the sound of an entity being hit by something that is not a player. */
    private fun announceEntityHit(
        watchers: List<PlayerSession>,
        entity: org.basaltmc.server.runtime.entity.ServerEntity,
        damageType: Identifier,
        causeEntityId: Int,
        died: Boolean,
    ) {
        val registryId = VanillaRegistryData.damageTypes.indexOfFirst { it.first == damageType.path }
        val sound = if (died) EntityCombat.deathSound(entity.type) else EntityCombat.hurtSound(entity.type)
        for (watcher in watchers) {
            if (registryId >= 0) {
                watcher.connection.sendPlayPacket(DamageEventPacket.ID) {
                    DamageEventPacket(entity.entityId, registryId, causeEntityId, causeEntityId).write(it)
                }
            }
            watcher.connection.sendPlayPacket(SetEntityMotionPacket.ID) {
                SetEntityMotionPacket(entity.entityId, entity.velocityX, entity.velocityY, entity.velocityZ).write(it)
            }
            watcher.connection.sendPlayPacket(SoundPacket.ID) {
                SoundPacket(
                    sound,
                    SoundSource.HOSTILE,
                    entity.x,
                    entity.y,
                    entity.z,
                    1f,
                    1f,
                    Random.nextLong(),
                ).write(it)
            }
            if (died) {
                watcher.connection.sendPlayPacket(EntityEventPacket.ID) {
                    EntityEventPacket(entity.entityId, EntityEventPacket.LIVING_DEATH.toByte()).write(it)
                }
            }
        }
        if (!died) return
        entity.killed = true
        entity.removed = true
    }

    /** parity: `Explosion.isSmall`, which is what picks the small flash over the big one. */
    private val largeExplosion = 2.0f

    private fun particleId(path: String): Int =
        org.basaltmc.server.registry.ParticleTypes[Identifier.vanilla(path)]
            ?.protocolId ?: 0

    /**
     * Sets off an explosion, drops what it broke and hurts whoever was standing in it.
     *
     * parity: `ServerExplosion`, the same shape as the bed's own: what it breaks drops with no tool
     * involved, and the neighbours of every hole are told afterwards.
     */
    private fun blowUp(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        x: Double,
        y: Double,
        z: Double,
        power: Float,
    ) {
        val broken = Explosions.explode(world, x, y, z, power, LegacyRandom(Random.nextLong()))
        // parity: `ServerExplosion.createDrops`, which gathers everything the blast broke and only
        // then drops it: what an explosion leaves is a handful of merged stacks, not one item per
        // block, and that is what a real client shows.
        val drops = mutableListOf<ExplosionDrop>()
        // parity: `ServerLevel.explode`'s own packet, which is the flash, the smoke thrown off the
        // blocks and the bang. The blocks themselves go out as ordinary updates below, so without
        // this an explosion was a wall of stone silently turning into air.
        val explode =
            ExplodePacket(
                x = x,
                y = y,
                z = z,
                radius = power,
                blockCount = broken.size,
                particleId = particleId(if (power < largeExplosion) "explosion" else "explosion_emitter"),
                sound = "entity.generic.explode",
                blockParticles =
                    listOf(
                        ExplosionParticle(particleId("poof"), ExplodePacket.POOF_SCALING, 1f),
                        ExplosionParticle(particleId("smoke"), ExplodePacket.SMOKE_SCALING, 1f),
                    ),
            )
        for (player in watchers) {
            player.connection.sendPlayPacket(ExplodePacket.ID) { explode.write(it) }
        }
        for (block in broken) {
            val at = BlockPos(block.x, block.y, block.z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(at, Blocks.AIR.stateId).write(it)
                }
            }
            Blocks.byStateId(block.stateId)?.let { collectExplodedBlock(world, it.block, at, power, drops) }
        }
        for (block in broken) {
            NeighbourUpdates.afterChange(
                world,
                block.x,
                block.y,
                block.z,
                onReshaped = { stateId, bx, by, bz ->
                    val at = BlockPos(bx, by, bz)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(at, stateId).write(it)
                        }
                    }
                },
            ) { stateId, bx, by, bz ->
                val at = BlockPos(bx, by, bz)
                for (player in watchers) {
                    player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                        ClientboundBlockUpdatePacket(at, Blocks.AIR.stateId).write(it)
                    }
                }
                // What the blast knocked loose afterwards falls as an ordinary break, not as part
                // of the explosion: parity with `dropResources`, which carries no radius.
                Blocks.byStateId(stateId)?.let { collectExplodedBlock(world, it.block, at, power = 0f, drops) }
            }
        }
        for (drop in drops) popResource(world, drop)
        for (player in watchers) {
            val distance =
                sqrt(
                    (player.x - x) * (player.x - x) + (player.y - y) * (player.y - y) + (player.z - z) * (player.z - z),
                )
            val damage = Explosions.damageAt(distance, power, power * 2f)
            if (damage <= 0f) continue
            player.connection.hurt(damage, "explosion", DamageTypes.EXPLOSION)
        }
    }

    /** One stack the blast is holding on to until it decides where it lands. */
    private class ExplosionDrop(
        val item: Identifier,
        var count: Int,
        val x: Int,
        val y: Int,
        val z: Int,
    )

    /**
     * What one exploded block leaves, merged into what the blast is already carrying.
     *
     * parity: `ServerExplosion.addOrAppendStack`, which merges into the first stack that will take
     * it, up to sixteen, and only starts a new one when none will.
     */
    private fun collectExplodedBlock(
        world: LoadedWorld,
        block: Identifier,
        pos: BlockPos,
        power: Float,
        drops: MutableList<ExplosionDrop>,
    ) {
        val table = lootTables.forBlock(block) ?: return
        // parity: `LootContextParams.EXPLOSION_RADIUS`, which `explosion_decay` turns into the one
        // in `power` chance each item has of surviving the blast.
        for (stack in table.generate(LegacyRandom(Random.nextLong()), explosionRadius = power)) {
            if (Items.REGISTRY.byIdentifier(stack.item) == null) continue
            var remaining = stack.count
            for (drop in drops) {
                if (drop.item != stack.item || drop.count >= EXPLOSION_MERGE_LIMIT) continue
                val moved = minOf(remaining, EXPLOSION_MERGE_LIMIT - drop.count)
                drop.count += moved
                remaining -= moved
                if (remaining <= 0) break
            }
            if (remaining > 0) drops += ExplosionDrop(stack.item, remaining, pos.x, pos.y, pos.z)
        }
    }

    /** parity: `Block.popResource`, which scatters the stack inside its own block and pops it up. */
    private fun popResource(
        world: LoadedWorld,
        drop: ExplosionDrop,
    ) {
        world.state.spawnItem(
            WorldState.DroppedItem(
                entityId = playerRegistry.allocateEntityId(),
                uuid = Uuid.random(),
                item = drop.item,
                count = drop.count,
                x = drop.x + 0.5 + jitter(),
                y = drop.y + 0.5 + jitter() - ITEM_HALF_HEIGHT,
                z = drop.z + 0.5 + jitter(),
                velocityX = Random.nextDouble() * POP_SPREAD - POP_SPREAD / 2.0,
                velocityY = POP_LIFT,
                velocityZ = Random.nextDouble() * POP_SPREAD - POP_SPREAD / 2.0,
            ),
        )
    }

    /** parity: `Mth.nextDouble(random, -0.25, 0.25)`, the quarter block a popped stack lands within. */
    private fun jitter(): Double = Random.nextDouble() * 2 * POP_JITTER - POP_JITTER

    private fun tickFallingBlocks(world: LoadedWorld) {
        val falling = world.state.entities().filter { !it.removed && it.type == FallingBlocks.ENTITY_TYPE }
        if (falling.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (entity in falling) {
            val landed = FallingBlocks.tick(world, entity) ?: continue
            val pos = BlockPos(landed.x, landed.y, landed.z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(pos, landed.stateId).write(it)
                }
            }
        }
    }

    /**
     * Lets the blocks vanilla picks at random act: crops grow, ice melts, cane climbs.
     *
     * parity: `ServerLevel.tickChunk`, which only runs over the chunks a player is near. Whoever is
     * in the world decides that here too, so an empty world sits still rather than growing on.
     */
    private fun tickRandomBlocks(world: LoadedWorld) {
        val speed = world.gameRules[GameRuleRegistry.RANDOM_TICK_SPEED]
        if (speed <= 0) return
        val chunks = simulatedChunks(world)
        if (chunks.isEmpty()) return
        val changes =
            randomTicks.tick(world, chunks, speed, LegacyRandom(Random.nextLong())) {
                playerRegistry.allocateEntityId()
            }
        if (changes.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (change in changes) {
            // parity: a block entity that changed its own state is a block change like any other,
            // so its neighbours hear about it: a daylight detector that dropped to zero at dusk has
            // to take the dust beside it down with it.
            announceRedstone(
                world,
                watchers,
                org.basaltmc.server.runtime.world
                    .BlockChange(change.x, change.y, change.z, change.stateId),
            )
            // parity: `setBlock` again: what a random tick changed reshapes what is around it, which
            // is how the grass under a snow layer that melted stops wearing snow.
            notifyAround(world, watchers, BlockPos(change.x, change.y, change.z))
        }
    }

    /**
     * Lets the block entities that have something to do act: today the three that smelt.
     *
     * parity: `ServerLevel.tickBlockEntities`, over the same chunks a random tick visits.
     */
    private fun tickBlockEntities(world: LoadedWorld) {
        val chunks = simulatedChunks(world)
        if (chunks.isEmpty()) return
        val changes =
            BlockEntityTicks.tick(
                world,
                chunks,
                { cooker, stack -> recipes.cookingFor(cooker, stack) },
                // parity: `CampfireBlockEntity.cookTick`, which drops what is done beside the fire.
                { done ->
                    world.state.spawnItem(
                        WorldState.DroppedItem(
                            entityId = playerRegistry.allocateEntityId(),
                            uuid = Uuid.random(),
                            item = done.stack.item,
                            count = done.stack.count,
                            x = done.x + 0.5,
                            y = done.y + 1.0,
                            z = done.z + 0.5,
                        ),
                    )
                },
            )
        if (changes.isEmpty()) return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (change in changes) {
            val pos = BlockPos(change.x, change.y, change.z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(pos, change.stateId).write(it)
                }
            }
        }
    }

    /**
     * The chunks around whoever is in the world, without repeats.
     *
     * parity: vanilla's simulation distance, which is ten by default and separate from the view
     * distance. This build has no setting of its own for it yet, so it follows the view distance up
     * to vanilla's own default and no further.
     */
    private fun simulatedChunks(world: LoadedWorld): List<IntArray> {
        val radius = minOf(world.settings.viewDistance, VANILLA_SIMULATION_DISTANCE)
        val seen = HashSet<Long>()
        val chunks = mutableListOf<IntArray>()
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            val centreX = player.x.toInt() shr 4
            val centreZ = player.z.toInt() shr 4
            for (x in centreX - radius..centreX + radius) {
                for (z in centreZ - radius..centreZ + radius) {
                    if (seen.add((x.toLong() shl 32) or (z.toLong() and 0xFFFFFFFFL))) chunks += intArrayOf(x, z)
                }
            }
        }
        return chunks
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
        if (world.weather.thundering) strikeLightning(world)
    }

    /**
     * The storm's own share of a tick: one roll per simulated chunk, and a strike where it lands.
     *
     * parity: `ServerLevel.tickChunk`, whose thunder is a one in a hundred thousand per chunk. A
     * lightning rod within 128 blocks takes the strike, which is the whole point of putting one up.
     */
    private fun strikeLightning(world: LoadedWorld) {
        val chunks = simulatedChunks(world)
        if (chunks.isEmpty()) return
        val random = LegacyRandom(Random.nextLong())
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (chunk in chunks) {
            if (!Lightning.shouldStrike(random)) continue
            val x = (chunk[0] shl 4) + random.nextInt(16)
            val z = (chunk[1] shl 4) + random.nextInt(16)
            val target = Lightning.targetFor(world, x, z) ?: continue
            val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
            // parity: `LightningBolt.tick`, where fire is only lit on normal and hard.
            val lightFires = world.difficulty == "normal" || world.difficulty == "hard"
            Lightning.strike(world, target[0], target[1], target[2], lightFires, changes)
            // parity: `GameEvent.LIGHTNING_STRIKE`, which a sculk sensor hears as fourteen.
            Vibrations.emit(
                world,
                "lightning_strike",
                target[0].toDouble(),
                target[1].toDouble(),
                target[2].toDouble(),
                changes,
            )
            for (change in changes) announceRedstone(world, watchers, change)
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
                    villagerTrades = villagerTrades,
                    jukeboxes = jukeboxes,
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

        /** parity: `EntityTypes.PLAYER`, whose box is what a mob's reach is measured against. */
        val PLAYER_TYPE: Identifier = Identifier.vanilla("player")

        /** parity: `death.attack.mob`, the message a mob with no name kills you with. */
        const val DEATH_MOB = "mob"

        /** What a cart with nobody in it is pushed by: nothing. */
        val EMPTY_INTENT = doubleArrayOf(0.0, 0.0)

        const val SOUND_TNT_PRIMED = "entity.tnt.primed"

        /** parity: `ThrownEnderpearl.onHit`, five points of fall damage on arrival. */
        const val ENDER_PEARL_DAMAGE = 5f
        const val SOUND_TELEPORT = "entity.player.teleport"
        const val DEATH_FALL = "fall"

        /** parity: `ItemEntity.merge(stack, input, 16)`, the cap on what one blast stacks together. */
        const val EXPLOSION_MERGE_LIMIT = 16

        /** parity: `Block.popResource` and `ItemEntity`'s own opening movement. */
        const val POP_JITTER = 0.25
        const val POP_SPREAD = 0.2
        const val POP_LIFT = 0.2
        const val ITEM_HALF_HEIGHT = 0.125

        /** parity: `death.attack.arrow`, and `AbstractArrow`'s own two points of damage. */
        const val DEATH_ARROW = "arrow"
        const val ARROW_BASE_DAMAGE = 2.0

        /** parity: `PanicGoal`, which runs for as long as the last hit is remembered. */
        const val PANIC_TICKS = 40

        /** parity: `canSeeSky`, which here is the full fifteen of sky light above the mob. */
        const val SKY_LIGHT_MAX = 15

        /** parity: `Creeper.explosionRadius`, and the two sounds its fuse and a bow make. */
        const val CREEPER_EXPLOSION_POWER = 3f
        const val SOUND_CREEPER_PRIMED = "entity.creeper.primed"
        const val SOUND_ENDERMAN_TELEPORT = "entity.enderman.teleport"
        const val SOUND_FIREBALL_SHOOT = "entity.ghast.shoot"
        const val SOUND_WITCH_THROW = "entity.witch.throw"
        const val SOUND_FANGS = "entity.evoker.prepare_attack"
        const val SOUND_SPLASH = "entity.splash_potion.break"
        const val SOUND_RAID_HORN = "event.raid.horn"

        /** parity: the horn is heard sixty-four blocks away, at sixty-four blocks of volume. */
        const val RAID_HORN_RANGE_SQUARED = 4096.0
        const val RAID_HORN_VOLUME = 64f
        const val RAID_BAR_KEY = "event.minecraft.raid"

        /** parity: `NUM_SPAWN_ATTEMPTS` and the ring `findRandomSpawnPos` looks along. */
        const val RAID_SPAWN_ATTEMPTS = 20
        const val RAID_SPAWN_MIN = 24.0
        const val RAID_SPAWN_MAX = 32.0
        const val RAID_SPAWN_LIFT = 12
        const val RAID_SPAWN_DROP = 24

        /** parity: `BadOmenMobEffect`, whose raid omen lasts thirty seconds. */
        const val RAID_OMEN_TICKS = 600

        /** How often a bad omen looks for a village, and what counts as one. */
        const val VILLAGE_CHECK_INTERVAL = 20
        const val VILLAGE_RADIUS = 16
        const val VILLAGE_HEIGHT = 4
        const val VILLAGE_BEDS = 3

        /** parity: `applySplash`, four blocks across and twenty seconds at the centre. */
        const val SPLASH_RANGE = 4.0
        const val SPLASH_DURATION = 400.0

        /** parity: `SpellcasterIllager`'s own spell byte, and the pose it puts the model in. */
        const val SPELL_INDEX = 17
        const val SPELL_CASTING = 1
        const val SPELL_NONE = 0

        /** parity: `Witch.performRangedAttack`, thrown at 0.75 with eight degrees of spread. */
        const val POTION_POWER = 0.75f
        const val POTION_SPREAD = 8f
        const val POTION_ARC = 0.2
        const val POTION_THROW_SHARE = 0.5

        /** parity: `EvokerAttackSpellGoal`, sixteen fangs walking out at six damage each. */
        const val FANG_STEPS = 16
        const val FANG_START = 1.5
        const val FANG_SPACING = 1.0
        const val FANG_HIT_RANGE = 1.0
        const val FANG_DAMAGE = 6f
        const val FANG_PARTICLES = 4
        const val FANG_PARTICLE_LIFT = 0.5

        val VEX_TYPE: Identifier = Identifier.vanilla("vex")

        /** parity: `Ghast.DATA_IS_CHARGING`, `Blaze`'s own flags byte and `Guardian`'s beam target. */
        const val GHAST_CHARGING_INDEX = 16
        const val BLAZE_CHARGED_INDEX = 16
        const val BLAZE_CHARGED_FLAG = 1
        const val GUARDIAN_BEAM_INDEX = 17

        /** parity: `Ghast.getExplosionPower`, and the speed a fireball is thrown at. */
        const val GHAST_EXPLOSION_POWER = 1f
        const val FIREBALL_POWER = 0.5f
        const val SMALL_FIREBALL_SPREAD = 4f
        const val FIREBALL_EYE_SHARE = 0.5

        /** parity: `SmallFireball.onHitEntity`, five points and five seconds of burning. */
        const val SMALL_FIREBALL_DAMAGE = 5f
        const val SMALL_FIREBALL_FIRE_TICKS = 100
        const val DEATH_ON_FIRE = "onFire"
        const val SOUND_SKELETON_SHOOT = "entity.skeleton.shoot"

        /** parity: `performRangedAttack`: from the eyes, aimed a third up the target, at 1.6. */
        const val ARROW_EYE_SHARE = 0.85
        const val ARROW_AIM_SHARE = 0.3333333333333333
        const val ARROW_ARC = 0.2
        const val ARROW_POWER = 1.6f

        /** parity: `AbstractArrow.onHitEntity`'s own `scale(0.2)` when the hit did not land. */
        const val ARROW_BOUNCE = 0.2

        // Beside worlds/, never inside one.
        const val PLAYERS_DATABASE = "players.db"

        // parity: vanilla's own clock resync interval, once a second.
        const val TIME_SYNC_INTERVAL_TICKS = 20L

        // parity: `ServerPlayer.tick` checks the border every ten ticks, not every one.
        const val BORDER_DAMAGE_INTERVAL_TICKS = 10L

        /** parity: `simulation-distance` in `server.properties`, ten chunks. */
        const val VANILLA_SIMULATION_DISTANCE = 10

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
