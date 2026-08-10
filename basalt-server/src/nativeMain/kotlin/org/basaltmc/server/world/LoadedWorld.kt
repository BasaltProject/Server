package org.basaltmc.server.world

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.timings.Timings
import org.basaltmc.common.timings.time
import org.basaltmc.server.config.WorldDeclaration
import org.basaltmc.server.config.WorldSettings
import org.basaltmc.server.config.WorldType
import org.basaltmc.world.Chunk
import org.basaltmc.world.FlatWorldGenerator
import org.basaltmc.world.NoiseWorldGenerator
import org.basaltmc.world.VoidWorldGenerator
import org.basaltmc.world.terrain.OverworldNoiseRouter
import org.basaltmc.world.terrain.RealTerrainGenerator
import org.basaltmc.world.terrain.VillageStructureGenerator
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.math.abs
import kotlin.time.Clock
import kotlin.time.ExperimentalTime
import kotlin.time.TimeSource

/**
 * One world, and everything that belongs to exactly one world.
 *
 * Before this existed the server had a single [WorldState], a single [RegionStore] and a single
 * generation queue held directly as fields, which is what made "several worlds" impossible rather
 * than merely unwritten. Bundling them here is the whole change: a world is now a thing you can
 * have more than one of, and the pieces that are genuinely per world (its chunks, its region files,
 * its generation workers, its settings, its seed) can no longer be reached without saying which
 * world you mean.
 *
 * What is still shared, correctly, is everything that is not per world: the player list, the
 * database, the command dispatcher.
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
    private val surfaceEnvironment: SurfaceRuleEnvironment? = null,
    private val villageGenerator: VillageStructureGenerator? = null,
    private val normalGenerator: ((chunkX: Int, chunkZ: Int) -> Chunk)? = null,
) {
    public val name: String get() = declaration.name

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

    public val regions: RegionStore =
        RegionStore(
            declaration.name,
            declaration.dimension.registryPath,
            worldsRoot,
            declaration.dimension.chunkHeight,
        )

    public val state: WorldState = WorldState(emptyList(), loadOrGenerate = ::loadOrGenerateChunk)

    public val generationQueue: ChunkGenerationQueue =
        ChunkGenerationQueue(state, declaration.dimension.registryPath, workerCount = generationWorkers)

    /** Where a player joining this world lands, resolved once the spawn column has been generated. */
    public var spawn: Triple<Double, Double, Double> = Triple(8.5, 64.0, 8.5)
        private set

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
                    check(declaration.dimension == org.basaltmc.server.config.WorldDimension.OVERWORLD) {
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
    ): Chunk = regions.loadChunk(chunkX, chunkZ) ?: generateChunk(chunkX, chunkZ)

    /**
     * Generates the small warm cache around spawn and reads the spawn height out of it.
     *
     * A void world skips the height lookup: its spawn is whatever the metadata says, because there
     * is no terrain to stand on other than the platform placed under exactly that point.
     */
    public fun warmSpawnArea(spawnRadiusChunks: Int) {
        val positions =
            (-spawnRadiusChunks..spawnRadiusChunks).flatMap { x ->
                (-spawnRadiusChunks..spawnRadiusChunks).map { z -> x to z }
            }
        generationQueue.awaitAll(positions)
        spawn =
            if (declaration.type == WorldType.VOID) {
                Triple(metadata.spawnX + 0.5, metadata.spawnY.toDouble(), metadata.spawnZ + 0.5)
            } else {
                val spawnChunk = state.getOrGenerateChunk(0, 0)
                Triple(8.5, spawnChunk.heightmap()[8 + 8 * 16].toDouble(), 8.5)
            }
    }

    @OptIn(ExperimentalTime::class)
    public fun save(chunks: List<Chunk>) {
        if (chunks.isEmpty()) return
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
        save(state.dirtyChunks())
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
                if (ChunkGenerationQueue.pack(chunkX, chunkZ) in pending) return@evictionCandidates true
                playerPositions.any { (x, z) -> abs(chunkX - x) <= retainRadius && abs(chunkZ - z) <= retainRadius }
            }
        generationQueue.retainWhere { chunkX, chunkZ ->
            playerPositions.any { (x, z) -> abs(chunkX - x) <= retainRadius && abs(chunkZ - z) <= retainRadius }
        }
        if (candidates.isEmpty()) return 0
        save(candidates.filter { it.isDirty })
        return state.evictClean(candidates)
    }

    public fun stopGenerating(drainTimeoutMillis: Long) {
        generationQueue.stop()
        generationQueue.awaitIdle(drainTimeoutMillis)
    }

    public fun flushRegions() {
        regions.flush()
    }
}
