package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.entity.EntityNbt
import org.basaltmc.server.runtime.sched.Owner
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.FlatWorldGenerator
import org.basaltmc.server.world.LeafDistances
import org.basaltmc.server.world.NoiseWorldGenerator
import org.basaltmc.server.world.VoidWorldGenerator
import org.basaltmc.server.world.terrain.OverworldBiomeSource
import org.basaltmc.server.world.terrain.OverworldNoiseRouter
import org.basaltmc.server.world.terrain.RealTerrainGenerator
import org.basaltmc.server.world.terrain.VillageStructureGenerator
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.common.timings.time
import org.basaltmc.wire.io.FileIO
import kotlin.math.abs
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/**
 * One world, and everything that belongs to exactly one world: its chunks, region files,
 * generation workers, settings and seed, none of them reachable without saying which world.
 *
 * The player list, the database and the command dispatcher stay shared, being not per world.
 */
public class LoadedWorld(
    public val declaration: WorldDeclaration,
    public val settings: WorldSettings,
    public val metadata: WorldMetadata,
    worldsRoot: String,
    private val logger: Logger,
    generationWorkers: Int,
    /** Where this world's generation and saving are recorded. Off unless `/timings` is on. */
    private val timings: Timings = Timings(),
    private val overworldRouter: OverworldNoiseRouter? = null,
    public val surfaceEnvironment: SurfaceRuleEnvironment? = null,
    public val villageGenerator: VillageStructureGenerator? = null,
    private val normalGenerator: ((chunkX: Int, chunkZ: Int) -> Chunk)? = null,
) {
    public val name: String get() = declaration.name

    /**
     * Who owns this world's state: its chunks, its blocks and everything living in them.
     *
     * The tick thread claims it on its first turn, and from then on anybody else reaches it with
     * [org.basaltmc.server.runtime.sched.Owner.submit] rather than by touching it. One world, one
     * owner today; one per region when regions are turned on, with the same rule.
     */
    public val owner: Owner = Owner(declaration.name)

    /**
     * What the client calls this world: a `ResourceKey<Level>`, not a dimension type.
     *
     * The distinction is what makes travelling between worlds work at all. The dimension type is
     * selected separately by [WorldDeclaration.dimension], while the level key identifies this
     * concrete world and tells the client when to rebuild its chunk cache.
     *
     * The default world keeps `minecraft:overworld` so an unmodified single-world setup looks
     * exactly as before, including in the client's own debug screen.
     */
    public val levelKey: String =
        if (declaration.isDefault) {
            declaration.dimension.registryKey
        } else {
            "basalt:" +
                declaration.name
                    .lowercase()
                    .map {
                        if (it.isLetterOrDigit() ||
                            it in "._-"
                        ) {
                            it
                        } else {
                            '_'
                        }
                    }.joinToString("")
        }

    /** This world's own directory, which is where `level.nbt` lives. */
    private val worldRoot: String = "$worldsRoot/${declaration.name}"

    public val regions: RegionStore =
        RegionStore(
            declaration.name,
            worldsRoot,
            declaration.dimension.chunkHeight,
        )

    init {
        // Said rather than migrated: a world that looks empty is worse than a line in the log.
        if (FileIO.directoryExists("$worldRoot/dimensions")) {
            logger.warn {
                "World '${declaration.name}' still has its regions under $worldRoot/dimensions/. " +
                    "They are read from $worldRoot/region/ now: move the .linear files there, or " +
                    "the world regenerates from its seed."
            }
        }
    }

    /** Where this world's portals are, including the ones in chunks nobody has loaded. */
    public val portals: PortalIndex = PortalIndex().also { it.load(worldRoot) }

    public val state: WorldState =
        WorldState(emptyList(), loadOrGenerate = ::loadOrGenerateChunk).also { it.owner = owner }

    public val generationQueue: ChunkGenerationQueue =
        ChunkGenerationQueue(state, declaration.dimension.registryPath, workerCount = generationWorkers)

    /** Where a player joining this world lands, resolved once the spawn column has been generated. */
    public var spawn: Triple<Double, Double, Double> = Triple(8.5, 64.0, 8.5)
        private set

    /** Where spawning puts a player from now on, and what `level.nbt` remembers. `/setworldspawn`. */
    public var spawnYaw: Float = 0f
        private set

    public fun setSpawn(
        x: Int,
        y: Int,
        z: Int,
        yaw: Float = 0f,
    ) {
        spawn = Triple(x + 0.5, y.toDouble(), z + 0.5)
        spawnYaw = yaw
        spawnOverride = Triple(x, y, z)
    }

    /** Null until somebody moves the spawn, so an untouched world keeps deriving it from terrain. */
    private var spawnOverride: Triple<Int, Int, Int>? = null

    /** Whether the stored spawn was chosen rather than derived, which is what a moved one looks like on disk. */
    private val movedSpawn: Boolean =
        Triple(metadata.spawnX, metadata.spawnY, metadata.spawnZ) != WorldMetadata.DEFAULT_SPAWN

    /**
     * This world's clock, weather and game rules: everything a `/time`, `/weather` or `/gamerule`
     * changes and a restart has to remember.
     *
     * Vanilla keeps one clock per *dimension type* on the server rather than one per level, and
     * weather and rules per level. Basalt keeps all three per world, which is the same thing on a
     * vanilla-shaped single-world server and the only sensible answer once an operator declares two
     * overworlds, something vanilla cannot do at all.
     */
    public val clock: WorldClock =
        WorldClock(
            totalTicks = metadata.timeOfDay,
            rate = metadata.clockRate,
            paused = metadata.clockPaused,
        )

    public val weather: WorldWeather =
        WorldWeather(
            raining = metadata.raining,
            rainTime = metadata.rainTime,
            thundering = metadata.thundering,
            thunderTime = metadata.thunderTime,
            clearWeatherTime = metadata.clearWeatherTime,
        )

    public val gameRules: GameRuleValues = metadata.gameRules

    /**
     * The biome source, when this world has a real one: what `/locate biome` searches with.
     *
     * Null for flat, void and generator-less worlds, where "the biome at a position" is whatever
     * the chunk was written with rather than something a sampler can answer.
     */
    public val biomeSource: OverworldBiomeSource? =
        overworldRouter?.takeIf { declaration.dimension == WorldDimension.OVERWORLD }?.let { OverworldBiomeSource(it) }

    /** This world's border. Not persisted yet, so it starts at vanilla's default size each run. */
    public val border: WorldBorder = WorldBorder()

    /**
     * Chunks `/forceload` keeps resident. Consulted by the unload sweep, which is the whole point:
     * a forced chunk stays generated whether or not a player is anywhere near it.
     */
    public val forcedChunks: MutableSet<Long> = mutableSetOf()

    /** Ticks this world has been running, which is not the same as its clock: `/time set` moves one, not the other. */
    public var gameTime: Long = metadata.gameTime
        private set

    /** What `/difficulty` last set, falling back to `worlds.toml`. See [WorldMetadata.difficulty]. */
    public var difficulty: String = metadata.difficulty ?: settings.difficulty

    public var difficultyLocked: Boolean = metadata.difficultyLocked

    /** What `/defaultgamemode` last set here, or null while it follows `server.toml`. */
    public var defaultGamemode: String? = metadata.defaultGamemode

    /** One tick of everything that advances on its own. Returns true when clients need a time update. */
    public fun tickTime(): Boolean {
        gameTime++
        // parity: only a dimension with a clock of its own has time to advance.
        val advanced =
            declaration.dimension.hasClock && clock.tick(gameRules[GameRuleRegistry.ADVANCE_TIME])
        // parity: `ServerLevel.advanceWeatherCycle` runs only where the dimension can have weather.
        if (declaration.dimension.canHaveWeather && gameRules[GameRuleRegistry.ADVANCE_WEATHER]) weather.tick()
        return advanced
    }

    /** Rewrites `level.nbt` with the state as it stands now. */
    public fun saveMetadata() {
        val (spawnX, spawnY, spawnZ) = spawnOverride ?: Triple(metadata.spawnX, metadata.spawnY, metadata.spawnZ)
        metadata
            .copy(
                spawnX = spawnX,
                spawnY = spawnY,
                spawnZ = spawnZ,
                timeOfDay = clock.totalTicks,
                gameTime = gameTime,
                clockRate = clock.rate,
                clockPaused = clock.paused,
                raining = weather.raining,
                rainTime = weather.rainTime,
                thundering = weather.thundering,
                thunderTime = weather.thunderTime,
                clearWeatherTime = weather.clearWeatherTime,
                gameRules = gameRules,
                difficulty = difficulty,
                difficultyLocked = difficultyLocked,
                defaultGamemode = defaultGamemode,
            ).writeTo(worldRoot)
    }

    /**
     * Terrain for one chunk, by the world's declared type.
     *
     * `normal` falls back to [NoiseWorldGenerator]'s approximation when no real router was supplied,
     * which is how protocol-focused tests avoid paying for real generation; anything that cares
     * about terrain fidelity passes a real one.
     */
    private fun generateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk = timings.of("world.generate").time { generateChunkNow(chunkX, chunkZ) }

    private fun generateChunkNow(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        when (declaration.type) {
            WorldType.FLAT -> FlatWorldGenerator.generate(chunkX, chunkZ, declaration.dimension.chunkHeight)
            WorldType.VOID ->
                VoidWorldGenerator.generate(
                    chunkX,
                    chunkZ,
                    spawnX = metadata.spawnX,
                    spawnY = metadata.spawnY,
                    spawnZ = metadata.spawnZ,
                    verticalBounds = declaration.dimension.chunkHeight,
                )
            WorldType.NORMAL, WorldType.AMPLIFIED -> {
                val generator = normalGenerator
                val router = overworldRouter
                val environment = surfaceEnvironment
                if (generator != null) {
                    generator(chunkX, chunkZ)
                } else if (router != null && environment != null) {
                    RealTerrainGenerator.generate(
                        router,
                        environment,
                        chunkX,
                        chunkZ,
                        villageGenerator = villageGenerator,
                    )
                } else {
                    check(declaration.dimension == org.basaltmc.server.runtime.config.WorldDimension.OVERWORLD) {
                        "Normal ${declaration.dimension} generation needs a vanilla generator factory"
                    }
                    NoiseWorldGenerator.generate(chunkX, chunkZ)
                }
            }
        }

    /** A chunk off disk if it has ever been saved, so exploring back over old ground keeps player edits. */
    private fun loadOrGenerateChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        (regions.loadChunk(chunkX, chunkZ) ?: generateChunk(chunkX, chunkZ)).also { chunk ->
            // What a chunk in memory says about its portals is always the truth; the index only
            // has to remember the chunks that are not.
            portals.scan(chunk)
            // Off the tick thread, and once: from here on a leaf knows its own distance.
            LeafDistances.assign(chunk)
        }

    /**
     * Generates the small warm cache around spawn and reads the spawn height out of it.
     *
     * A void world skips the height lookup: its spawn is whatever the metadata says, because there
     * is no terrain to stand on other than the platform placed under exactly that point. A world
     * whose spawn was moved by `/setworldspawn` skips it too: the point of moving it is that it is
     * no longer wherever the terrain at the origin happens to be.
     */
    public fun warmSpawnArea(spawnRadiusChunks: Int) {
        val positions =
            (-spawnRadiusChunks..spawnRadiusChunks).flatMap { x ->
                (-spawnRadiusChunks..spawnRadiusChunks).map { z -> x to z }
            }
        generationQueue.awaitAll(positions)
        spawn =
            if (declaration.type == WorldType.VOID || movedSpawn) {
                Triple(metadata.spawnX + 0.5, metadata.spawnY.toDouble(), metadata.spawnZ + 0.5)
            } else {
                val spawnChunk = state.getOrGenerateChunk(0, 0)
                Triple(8.5, spawnChunk.heightmap()[8 + 8 * 16].toDouble(), 8.5)
            }
    }

    /**
     * Puts the entities standing in each chunk into it, so they go out with its blocks.
     *
     * parity: vanilla saves entities with the chunk they are in (in its own region folder since
     * 1.17). Without this a mob only existed for as long as the process did.
     */
    private fun stashEntities(chunks: List<Chunk>) {
        for (chunk in chunks) {
            val entities = state.entitiesIn(chunk.chunkX, chunk.chunkZ)
            if (entities.isEmpty() && chunk.savedEntities().isEmpty()) continue
            chunk.setSavedEntities(entities.filter { !it.removed }.map { EntityNbt.encode(it) })
        }
    }

    @OptIn(ExperimentalTime::class)
    public fun save(chunks: List<Chunk>) {
        if (chunks.isEmpty()) return
        stashEntities(chunks)
        val startedAt = TimeSource.Monotonic.markNow()
        val written =
            timings.of("world.save").time {
                regions.saveChunks(
                    chunks,
                    Clock.System
                        .now()
                        .epochSeconds
                        .toInt(),
                )
            }
        logger.debug {
            "[$name] saved ${chunks.size} chunk(s) across $written region(s) in " +
                "${startedAt.elapsedNow().inWholeMilliseconds}ms (${regions.encodeMillis}ms encoding, " +
                "${regions.writeMillis}ms writing, ${regions.cachedRegionCount} regions cached, " +
                "${state.loadedChunkCount} loaded)."
        }
    }

    public fun saveDirty() {
        // A mob walking across a chunk nothing else touched still has to be written down, so the
        // chunks holding entities join whatever the blocks made dirty.
        val chunks = state.dirtyChunks().toMutableList()
        val already = chunks.mapTo(mutableSetOf()) { it.chunkX to it.chunkZ }
        // A chunk that carried a mob last time has to be written again once it walks away, or the
        // old copy on disk comes back beside the one that moved.
        for (chunk in state.chunksWithStashedEntities()) {
            if (already.add(chunk.chunkX to chunk.chunkZ)) chunks += chunk
        }
        for (position in state.chunksWithEntities()) {
            if (!already.add(position)) continue
            state.chunkIfLoaded(position.first, position.second)?.let { chunks += it }
        }
        save(chunks)
        portals.save(worldRoot)
    }

    /**
     * Drops chunks no player in *this* world can reach, after saving whatever changed in them.
     *
     * [playerPositions] is only the players who are in this world: keeping a chunk resident because
     * somebody is standing at the same coordinates in a different world would be nonsense.
     */
    public fun unloadUnreachable(
        playerPositions: List<Pair<Int, Int>>,
        spawnRadiusChunks: Int,
        retainMarginChunks: Int,
    ): Int {
        val retainRadius = settings.viewDistance + retainMarginChunks
        val pending = generationQueue.pendingKeys()
        val candidates =
            state.evictionCandidates { chunkX, chunkZ ->
                if (abs(chunkX) <= spawnRadiusChunks && abs(chunkZ) <= spawnRadiusChunks) return@evictionCandidates true
                if (ChunkGenerationQueue.pack(chunkX, chunkZ) in forcedChunks) return@evictionCandidates true
                if (ChunkGenerationQueue.pack(chunkX, chunkZ) in pending) return@evictionCandidates true
                playerPositions.any { (x, z) -> abs(chunkX - x) <= retainRadius && abs(chunkZ - z) <= retainRadius }
            }
        generationQueue.retainWhere { chunkX, chunkZ ->
            playerPositions.any { (x, z) -> abs(chunkX - x) <= retainRadius && abs(chunkZ - z) <= retainRadius }
        }
        if (candidates.isEmpty()) return 0
        // Entities go to disk with the chunk they stand in, and then out of memory with it: leaving
        // them behind would spawn a second copy when the chunk came back.
        val withEntities = candidates.filter { state.entitiesIn(it.chunkX, it.chunkZ).isNotEmpty() }
        save(candidates.filter { it.isDirty } + withEntities.filter { !it.isDirty })
        val evicted = state.evictClean(candidates)
        for (chunk in withEntities) state.forgetEntitiesIn(chunk.chunkX, chunk.chunkZ)
        return evicted
    }

    public fun stopGenerating(drainTimeoutMillis: Long) {
        generationQueue.stop()
        generationQueue.awaitIdle(drainTimeoutMillis)
    }

    public fun flushRegions() {
        regions.flush()
    }
}
