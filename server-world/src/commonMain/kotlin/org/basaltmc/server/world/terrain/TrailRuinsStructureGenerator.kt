package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.structure.JigsawAssembly
import org.basaltmc.server.world.structure.PlacedStructurePiece
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureProjection
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.server.world.structure.TemplateLoader
import kotlin.concurrent.AtomicReference
import kotlin.math.abs

/**
 * The trail ruins: one jigsaw start from `trail_ruins/tower`, buried rather than bearded. // parity:
 * JigsawStructure over the `trail_ruins` set. `bury` fills ground back over the pieces instead of
 * clearing a hollow under them, so only their top course shows; it starts 15 blocks below the map.
 */
public class TrailRuinsStructureGenerator(
    private val router: OverworldNoiseRouter,
    private val grid: StructurePlacementGrid,
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val ruinBiomes: Set<String>,
    private val poolLookup: (String) -> StructureTemplatePool?,
    private val templateLoader: TemplateLoader,
) {
    private val cache = AtomicReference<Map<CandidateKey, List<PlacedStructurePiece>>>(emptyMap())

    /** Cached candidate count and their total placed pieces, for the timings memory breakdown. */
    public fun assemblyStats(): Pair<Int, Long> {
        val current = cache.value
        var pieces = 0L
        for (start in current.values) pieces += start.size
        return current.size to pieces
    }

    public fun generateInto(
        chunk: Chunk,
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ) {
        val clip = chunkClip(chunkX, chunkZ, 0)
        val starts = nearbyStarts(chunkX, chunkZ, seed, clip)
        if (starts.isEmpty()) return
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunkX * 16, chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, UNDERGROUND_STRUCTURES_STEP)
        for ((origin, pieces) in starts) {
            recordStart(chunk, clip, origin, pieces)
            for (piece in pieces) {
                if (!piece.boundingBox.intersects(clip)) continue
                piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, random, seed)
            }
        }
    }

    /** [base] plus this chunk's beard, or [base] unchanged when no outpost reaches it. */
    public fun beardified(
        base: DensityFunction,
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): DensityFunction {
        val nearClip = chunkClip(chunkX, chunkZ, BEARD_RADIUS)
        val chunkMinX = chunkX * 16
        val chunkMinZ = chunkZ * 16
        val rigids = mutableListOf<StructureBeardifier.Rigid>()
        val junctions = mutableListOf<StructureBeardifier.Junction>()
        for ((_, pieces) in nearbyStarts(chunkX, chunkZ, seed, nearClip)) {
            for (piece in pieces) {
                if (!piece.boundingBox.intersects(nearClip)) continue
                if (piece.element.projection == StructureProjection.RIGID) {
                    rigids +=
                        StructureBeardifier.Rigid(
                            piece.boundingBox,
                            piece.groundLevelDelta,
                            TerrainAdjustment.BURY,
                        )
                }
                for (junction in piece.junctions) {
                    if (junction.sourceX > chunkMinX - BEARD_RADIUS &&
                        junction.sourceX < chunkMinX + 15 + BEARD_RADIUS &&
                        junction.sourceZ > chunkMinZ - BEARD_RADIUS &&
                        junction.sourceZ < chunkMinZ + 15 + BEARD_RADIUS
                    ) {
                        junctions +=
                            StructureBeardifier.Junction(junction.sourceX, junction.sourceGroundY, junction.sourceZ)
                    }
                }
            }
        }
        return if (rigids.isEmpty() && junctions.isEmpty()) {
            base
        } else {
            DensityFunctions.add(base, StructureBeardifier(rigids, junctions))
        }
    }

    /** The nearest chunk holding an outpost, searched outward over the grid's own cells. */
    public fun locate(
        seed: Long,
        fromChunkX: Int,
        fromChunkZ: Int,
        maxCells: Int = DEFAULT_SEARCH_CELLS,
    ): Pair<Int, Int>? {
        val cellX = fromChunkX.floorDiv(grid.spacing)
        val cellZ = fromChunkZ.floorDiv(grid.spacing)
        for (ring in 0..maxCells) {
            for (dx in -ring..ring) {
                for (dz in -ring..ring) {
                    if (ring != 0 && abs(dx) != ring && abs(dz) != ring) continue
                    val (candidateX, candidateZ) =
                        grid.candidateChunk(seed, (cellX + dx) * grid.spacing, (cellZ + dz) * grid.spacing)
                    if (assembledStart(candidateX, candidateZ, seed).isEmpty()) continue
                    return candidateX to candidateZ
                }
            }
        }
        return null
    }

    private fun recordStart(
        chunk: Chunk,
        clip: StructureBoundingBox,
        origin: CandidateKey,
        pieces: List<PlacedStructurePiece>,
    ) {
        val here = pieces.filter { it.boundingBox.intersects(clip) }
        if (here.isEmpty()) return
        val whole = StructureStartRecord.of(NAME, 0, 0, pieces.map { it.boundingBox }) ?: return
        chunk.structures.add(
            StructureStartRecord(NAME, origin.chunkX, origin.chunkZ, whole.fullBounds, here.map { it.boundingBox }),
        )
    }

    private fun chunkClip(
        chunkX: Int,
        chunkZ: Int,
        margin: Int,
    ) = StructureBoundingBox(
        chunkX * 16 - margin,
        WorldHeight.MIN_Y,
        chunkZ * 16 - margin,
        chunkX * 16 + 15 + margin,
        WorldHeight.MAX_Y,
        chunkZ * 16 + 15 + margin,
    )

    private fun nearbyStarts(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
        clip: StructureBoundingBox,
    ): List<Pair<CandidateKey, List<PlacedStructurePiece>>> {
        val cellX = chunkX.floorDiv(grid.spacing)
        val cellZ = chunkZ.floorDiv(grid.spacing)
        val reachChunks = (MAX_DISTANCE_FROM_CENTER + BEARD_RADIUS) / 16 + 1
        val starts = mutableListOf<Pair<CandidateKey, List<PlacedStructurePiece>>>()
        for (cellOffsetX in -1..1) {
            for (cellOffsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    grid.candidateChunk(
                        seed,
                        (cellX + cellOffsetX) * grid.spacing,
                        (cellZ + cellOffsetZ) * grid.spacing,
                    )
                if (abs(candidateX - chunkX) > reachChunks || abs(candidateZ - chunkZ) > reachChunks) continue
                val pieces = assembledStart(candidateX, candidateZ, seed)
                if (pieces.isEmpty()) continue
                if (pieces.any { it.boundingBox.intersects(clip) }) {
                    starts += CandidateKey(seed, candidateX, candidateZ) to pieces
                }
            }
        }
        return starts
    }

    private fun assembledStart(
        candidateChunkX: Int,
        candidateChunkZ: Int,
        seed: Long,
    ): List<PlacedStructurePiece> {
        val key = CandidateKey(seed, candidateChunkX, candidateChunkZ)
        cache.value[key]?.let { return it }

        val samplers = HashMap<Long, DensityChunkSampler>()
        val assembled =
            if (!grid.isStructureChunk(seed, candidateChunkX, candidateChunkZ) ||
                !inRuinBiome(candidateChunkX, candidateChunkZ, samplers)
            ) {
                emptyList()
            } else {
                val random = WorldgenRandom(LegacyRandom(0L))
                random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
                JigsawAssembly(poolLookup, templateLoader, heightAt = { x, z -> surfaceHeightAt(x, z, samplers) })
                    .assemble(
                        START_POOL,
                        StructurePos(candidateChunkX * 16, START_HEIGHT, candidateChunkZ * 16),
                        MAX_DEPTH,
                        random,
                        projectStartToHeightmap = true,
                        maxDistanceFromCenter = MAX_DISTANCE_FROM_CENTER,
                        useExpansionHack = false,
                    )
            }

        while (true) {
            val current = cache.value
            current[key]?.let { return it }
            val next =
                if (current.size >= CACHE_CAPACITY) mapOf(key to assembled) else current + (key to assembled)
            if (cache.compareAndSet(current, next)) return assembled
        }
    }

    /**
     * // parity, narrowed the same way [VillageStructureGenerator] narrows it: the jar tests the
     * biome at the centre of the start piece's box, which is only known once the piece is placed.
     * The candidate chunk's own centre column stands in for it.
     */
    private fun inRuinBiome(
        candidateChunkX: Int,
        candidateChunkZ: Int,
        samplers: HashMap<Long, DensityChunkSampler>,
    ): Boolean {
        val centreX = candidateChunkX * 16 + 8
        val centreZ = candidateChunkZ * 16 + 8
        // The tag keeps its namespace and so does the biome name, so neither side is stripped.
        return biomeAt(centreX, surfaceHeightAt(centreX, centreZ, samplers), centreZ) in ruinBiomes
    }

    private fun surfaceHeightAt(
        x: Int,
        z: Int,
        samplers: HashMap<Long, DensityChunkSampler>,
    ): Int {
        val chunkX = x shr 4
        val chunkZ = z shr 4
        val key = (chunkX.toLong() and 0xFFFFFFFFL) or (chunkZ.toLong() shl 32)
        val sampler =
            samplers.getOrPut(key) {
                DensityChunkSampler(
                    router.finalDensity,
                    chunkX,
                    chunkZ,
                    WorldHeight.MIN_Y,
                    WorldHeight.HEIGHT,
                    CELL_WIDTH,
                    CELL_HEIGHT,
                )
            }
        val localX = x and 15
        val localZ = z and 15
        var y = WorldHeight.MAX_Y
        while (y >= WorldHeight.MIN_Y && sampler.sample(localX, y, localZ) <= 0.0) y--
        return (y + 1).coerceAtLeast(SEA_LEVEL)
    }

    private data class CandidateKey(
        val seed: Long,
        val chunkX: Int,
        val chunkZ: Int,
    )

    public companion object {
        public const val NAME: String = "trail_ruins"

        /** Fourth of the five structures of `underground_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 3
        public const val UNDERGROUND_STRUCTURES_STEP: Int = 3

        public const val MAX_DEPTH: Int = 7
        public const val MAX_DISTANCE_FROM_CENTER: Int = 80
        public const val START_POOL: String = "minecraft:trail_ruins/tower"

        /** // parity: `start_height: {absolute: -15}`, which draws nothing from the random. */
        private const val START_HEIGHT = -15
        private const val BEARD_RADIUS = 12
        private const val CACHE_CAPACITY = 32
        private const val CELL_WIDTH = 4
        private const val CELL_HEIGHT = 8
        private const val SEA_LEVEL = 63
        private const val DEFAULT_SEARCH_CELLS = 64
    }
}
