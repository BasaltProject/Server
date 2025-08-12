package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.runtime.command.CommandDispatcher
import org.basaltmc.server.runtime.config.BasaltConfig
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.config.WorldsConfig
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.plugins.ServerPluginServices
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.sched.RegionScheduler
import org.basaltmc.server.runtime.world.ChunkGenerationQueue
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.VanillaWorldGeneratorFactory
import org.basaltmc.server.runtime.world.WorldMetadata
import org.basaltmc.server.world.terrain.OverworldNoiseRouter
import org.basaltmc.server.world.terrain.VillageStructureGenerator
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.io.FileIO

/**
 * Every world this server knows about: what builds one, what makes a new one at runtime, the
 * scheduler they all share, and what `/reload` has to redo once the configuration underneath
 * them changes. Split out of [BasaltServer] so the shape of "a world" has one owner.
 *
 * [pluginEvents], [commands] and [pluginServices] are read lazily: this registry is built before
 * any of the three exist, and by the time [createWorld] or [onConfigsReloaded] actually runs,
 * they do.
 */
public class WorldRegistry(
    private val config: BasaltConfig,
    private val worldsConfig: WorldsConfig,
    private val overworldRouter: OverworldNoiseRouter?,
    private val surfaceEnvironment: SurfaceRuleEnvironment?,
    private val villageGenerator: VillageStructureGenerator?,
    private val worldGeneratorFactory: VanillaWorldGeneratorFactory?,
    private val worldsRoot: String,
    private val logger: Logger,
    private val timings: Timings,
    defaultWorldMetadata: WorldMetadata?,
    private val pluginEvents: () -> PluginEventBus,
    private val commands: () -> CommandDispatcher,
    private val pluginServices: () -> ServerPluginServices,
    private val playerRegistry: PlayerRegistry,
) {
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

    /** The chunk-set version each world was last split on, so an unchanged world is not walked. */
    internal val lastChunkSetVersion = HashMap<String, Long>()

    /**
     * Who hands out regions and owns what belongs to no region.
     *
     * One per server, shared by every world: with `regional` off each world is one region and they
     * take turns on this thread, which is the order of tick vanilla has. Turning it on is what
     * splits a world into regions that tick at once.
     */
    internal val regionScheduler: RegionScheduler =
        RegionScheduler(regional = config.regional, threads = config.regionThreads)

    /**
     * Every world, each with its own chunks, region files, generation workers, settings and seed.
     * Keyed by name, which is what a player's stored row refers to.
     *
     * Replaced rather than added to, and that is not a style choice: the tick loop walks these while
     * a plugin can be making one, and inserting into a map somebody is iterating is a crash on the
     * tick thread. Swapping the reference leaves whoever is mid-walk on the old one, which is
     * exactly right: a world made this turn starts ticking the next.
     */
    internal var worlds: Map<String, LoadedWorld> =
        worldsConfig.worlds.associate { declaration ->
            declaration.name to buildWorld(declaration, worldMetadata.getValue(declaration.name))
        }

    /**
     * One world, built the same way whether it came from `worlds.toml` or from a plugin.
     *
     * Extracted so there is one place that knows what a world needs; a world made at runtime that
     * skipped one of these would be subtly different from the ones declared in the file, and the
     * difference would show up as a bug in generation months later.
     */
    private fun buildWorld(
        declaration: WorldDeclaration,
        metadata: WorldMetadata,
    ): LoadedWorld {
        val generators =
            if (declaration.type == WorldType.NORMAL || declaration.type == WorldType.AMPLIFIED) {
                worldGeneratorFactory?.create(declaration, metadata)
            } else {
                null
            }
        return LoadedWorld(
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
            scheduler = regionScheduler,
        )
    }

    /**
     * Makes a world that was not in `worlds.toml`, the way a plugin asks for one.
     *
     * A world like any other: its own folder, `settings.toml` and seed. Null if the name is taken
     * or unusable, because a world that half exists is worse than none.
     *
     * Its spawn is not generated up front, unlike the declared ones: this runs inside a tick, and
     * forty-nine chunks there is a freeze every player would feel.
     */
    internal fun createWorld(
        name: String,
        dimension: WorldDimension,
        type: WorldType,
        seed: Long?,
    ): LoadedWorld? {
        if (name in worlds || !name.matches(WORLD_NAME_SHAPE)) return null
        val declaration =
            WorldDeclaration(
                name = name,
                dimension = dimension,
                type = type,
                seed = seed,
                isDefault = false,
            )
        FileIO.createDirectories("$worldsRoot/$name")
        val metadata =
            WorldMetadata.loadOrCreate(
                "$worldsRoot/$name",
                seed ?: defaultWorld.metadata.seed,
                WorldMetadata.DEFAULT_SPAWN,
                logger,
            )
        val world = buildWorld(declaration, metadata)
        worlds = worlds + (name to world)
        logger.info { "Made world '$name' ($dimension, ${type.name.lowercase()}, seed ${metadata.seed})." }
        world.saveDirty()
        // Next turn, not now: this is usually called from inside a plugin's own handler, and firing
        // an event there would re-enter the Lua state that is already running. One turn later it is
        // an ordinary event like any other.
        regionScheduler.global.schedule(0) {
            pluginEvents().emit(PluginEventTypes.WORLD_LOAD) { event -> event.set(0, name) }
        }
        return world
    }

    internal val defaultWorld: LoadedWorld = worlds.getValue(worldsConfig.default.name)

    internal val worldSeed: Long get() = defaultWorld.metadata.seed

    /**
     * What has to be told that the configuration changed.
     *
     * The tab list, scoreboard, boss bar and server list all read their file on every use, so they
     * need nothing. The command tree does not: a command's permission and aliases are baked in when
     * it is registered, and the client holds its own copy for autocompletion, so both have to be
     * built again and sent out.
     */
    internal fun onConfigsReloaded() {
        commands().rebuild()
        pluginServices().refreshCommands()
        for (player in playerRegistry.snapshot()) {
            runCatching { player.connection.sendCommandTree() }
        }
    }

    private companion object {
        /** What a world may be called: it becomes a folder name and a row in every player's file. */
        val WORLD_NAME_SHAPE = Regex("[a-zA-Z0-9_-]{1,64}")
    }
}
