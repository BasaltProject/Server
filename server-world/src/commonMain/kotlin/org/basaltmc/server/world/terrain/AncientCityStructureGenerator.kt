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
 * Places ancient cities: fixed depth, anchored on a named jigsaw block rather than the template's own
 * origin. // parity: the `ancient_cities` set. Its `beard_box` is what hollows the deepslate out, so
 * [finalDensityForChunk] has to be folded into terrain generation.
 */
public class AncientCityStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val poolLookup: (String) -> StructureTemplatePool?,
    private val templateLoader: TemplateLoader,
    private val cityBiomes: Set<String>,
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
        val clip = chunkClip(chunkX, chunkZ)
        val starts = nearbyStarts(chunkX, chunkZ, seed, clip)
        if (starts.isEmpty()) return
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunkX * 16, chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, UNDERGROUND_DECORATION_STEP)
        for (pieces in starts) {
            recordStart(chunk, pieces)
            for (piece in pieces) {
                if (!piece.boundingBox.intersects(clip)) continue
                piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, random)
            }
        }
    }

    /**
     * [base] plus this chunk's beard, or [base] unchanged when no city reaches it. // parity: Beardifier.
     * Takes the density so a chunk with a village too gets one beardifier, not two bases.
     */
    public fun beardified(
        base: DensityFunction,
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): DensityFunction {
        val nearClip =
            StructureBoundingBox(
                chunkX * 16 - BEARD_RADIUS,
                WorldHeight.MIN_Y,
                chunkZ * 16 - BEARD_RADIUS,
                chunkX * 16 + 15 + BEARD_RADIUS,
                WorldHeight.MAX_Y,
                chunkZ * 16 + 15 + BEARD_RADIUS,
            )
        val rigids = mutableListOf<StructureBeardifier.Rigid>()
        val junctions = mutableListOf<StructureBeardifier.Junction>()
        // The starts are filtered with the inflated box, not the chunk itself: a chunk with no
        // piece of its own still gets the beard of the pieces within twelve blocks of it, and
        // asking for the strict chunk dropped those starts before the beard could look at them.
        for (pieces in nearbyStarts(chunkX, chunkZ, seed, nearClip)) {
            for (piece in pieces) {
                if (!piece.boundingBox.intersects(nearClip)) continue
                // parity: a junction contributes whatever the piece's own projection is, so these
                // are collected before the rigid-only test below, not after it.
                for (junction in piece.junctions) {
                    if (junction.sourceX <= chunkX * 16 - BEARD_RADIUS) continue
                    if (junction.sourceX >= chunkX * 16 + 15 + BEARD_RADIUS) continue
                    if (junction.sourceZ <= chunkZ * 16 - BEARD_RADIUS) continue
                    if (junction.sourceZ >= chunkZ * 16 + 15 + BEARD_RADIUS) continue
                    junctions +=
                        StructureBeardifier.Junction(junction.sourceX, junction.sourceGroundY, junction.sourceZ)
                }
                if (piece.element.projection != StructureProjection.RIGID) continue
                rigids +=
                    StructureBeardifier.Rigid(
                        piece.boundingBox,
                        piece.groundLevelDelta,
                        TerrainAdjustment.BEARD_BOX,
                    )
            }
        }
        if (rigids.isEmpty() && junctions.isEmpty()) return base
        return DensityFunctions.add(base, StructureBeardifier(rigids, junctions))
    }

    /**
     * Records the slice of this start that reaches [chunk], for later position queries. The full
     * bounds come from every piece, the piece list only from the ones this chunk holds.
     */
    private fun recordStart(
        chunk: Chunk,
        pieces: List<PlacedStructurePiece>,
    ) {
        val chunkBox = chunkClip(chunk.chunkX, chunk.chunkZ)
        val here = pieces.filter { it.boundingBox.intersects(chunkBox) }
        if (here.isEmpty()) return
        val whole = StructureStartRecord.of(NAME, 0, 0, pieces.map { it.boundingBox }) ?: return
        chunk.structures.add(
            StructureStartRecord(
                NAME,
                pieces[0].position.x shr 4,
                pieces[0].position.z shr 4,
                whole.fullBounds,
                here.map { it.boundingBox },
            ),
        )
    }

    private fun chunkClip(
        chunkX: Int,
        chunkZ: Int,
    ) = StructureBoundingBox(
        chunkX * 16,
        WorldHeight.MIN_Y,
        chunkZ * 16,
        chunkX * 16 + 15,
        WorldHeight.MAX_Y,
        chunkZ * 16 + 15,
    )

    /**
     * A city reaches 116 blocks from its centre, so a chunk has to consider candidates several
     * cells away rather than only its own.
     */
    private fun nearbyStarts(
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
        clip: StructureBoundingBox,
    ): List<List<PlacedStructurePiece>> {
        val cellX = chunkX.floorDiv(GRID.spacing)
        val cellZ = chunkZ.floorDiv(GRID.spacing)
        val reachChunks = (MAX_DISTANCE_FROM_CENTER + BEARD_RADIUS) / 16 + 1
        val starts = mutableListOf<List<PlacedStructurePiece>>()
        for (cellOffsetX in -1..1) {
            for (cellOffsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    GRID.candidateChunk(
                        seed,
                        (cellX + cellOffsetX) * GRID.spacing,
                        (cellZ + cellOffsetZ) * GRID.spacing,
                    )
                if (abs(candidateX - chunkX) > reachChunks || abs(candidateZ - chunkZ) > reachChunks) continue
                val pieces = assembledStart(candidateX, candidateZ, seed)
                if (pieces.isEmpty()) continue
                if (pieces.any { it.boundingBox.intersects(clip) }) starts += pieces
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

        val assembled =
            if (!GRID.isStructureChunk(seed, candidateChunkX, candidateChunkZ) ||
                biomeAt(candidateChunkX * 16 + 8, START_Y, candidateChunkZ * 16 + 8) !in cityBiomes
            ) {
                emptyList()
            } else {
                val random = WorldgenRandom(LegacyRandom(0L))
                random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
                JigsawAssembly(poolLookup, templateLoader, heightAt = { _, _ -> START_Y })
                    .assemble(
                        START_POOL,
                        StructurePos(candidateChunkX * 16, START_Y, candidateChunkZ * 16),
                        MAX_DEPTH,
                        random,
                        projectStartToHeightmap = false,
                        maxDistanceFromCenter = MAX_DISTANCE_FROM_CENTER,
                        startJigsawName = START_JIGSAW,
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

    private data class CandidateKey(
        val seed: Long,
        val chunkX: Int,
        val chunkZ: Int,
    )

    internal companion object {
        const val START_POOL = "minecraft:ancient_city/city_center"
        const val START_JIGSAW = "minecraft:city_anchor"
        const val START_Y = -27
        const val MAX_DEPTH = 7
        const val MAX_DISTANCE_FROM_CENTER = 116
        const val BEARD_RADIUS = 12
        const val CACHE_CAPACITY = 16

        /** First of `underground_decoration`'s three structures, ahead of the fortress and the fossil. */
        const val NAME = "ancient_city"
        const val STRUCTURE_INDEX = 0
        const val UNDERGROUND_DECORATION_STEP = 7

        val GRID = StructurePlacementGrid(spacing = 24, separation = 8, salt = 20083232)
    }
}
