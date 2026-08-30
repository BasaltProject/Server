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
import org.basaltmc.server.world.structure.StructureTemplatePool
import org.basaltmc.server.world.structure.TemplateLoader
import kotlin.concurrent.AtomicReference
import kotlin.math.abs

/**
 * Places trial chambers: like an ancient city it never looks at the surface, but its start height is
 * drawn and it has no named starting jigsaw. // parity: the `trial_chambers` set. Its `encapsulate`
 * adds density rather than carving, so [beardified] folds into terrain generation like the city's.
 */
public class TrialChamberStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val poolLookup: (String) -> StructureTemplatePool?,
    private val templateLoader: TemplateLoader,
    private val chamberBiomes: Set<String>,
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
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, UNDERGROUND_STRUCTURES_STEP)
        for (pieces in starts) {
            for (piece in pieces) {
                if (!piece.boundingBox.intersects(clip)) continue
                piece.element.place(templateLoader, chunk, piece.position, piece.rotation, clip, random)
            }
        }
    }

    /**
     * [base] plus this chunk's beard, or [base] unchanged when none reaches it. // parity: Beardifier.
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
                        TerrainAdjustment.ENCAPSULATE,
                    )
            }
        }
        if (rigids.isEmpty() && junctions.isEmpty()) return base
        return DensityFunctions.add(base, StructureBeardifier(rigids, junctions))
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
                biomeAt(candidateChunkX * 16 + 8, MIN_START_Y, candidateChunkZ * 16 + 8) !in chamberBiomes
            ) {
                emptyList()
            } else {
                val random = WorldgenRandom(LegacyRandom(0L))
                random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
                // parity: the start height is a uniform provider, so it costs a draw before the
                // assembly's own; a fixed depth like the city's costs none.
                val startY = MIN_START_Y + random.nextInt(MAX_START_Y - MIN_START_Y + 1)
                JigsawAssembly(poolLookup, templateLoader, heightAt = { _, _ -> startY })
                    .assemble(
                        START_POOL,
                        StructurePos(candidateChunkX * 16, startY, candidateChunkZ * 16),
                        MAX_DEPTH,
                        random,
                        projectStartToHeightmap = false,
                        maxDistanceFromCenter = MAX_DISTANCE_FROM_CENTER,
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
        const val START_POOL = "minecraft:trial_chambers/chamber/end"
        const val MIN_START_Y = -40
        const val MAX_START_Y = -20
        const val MAX_DEPTH = 20
        const val MAX_DISTANCE_FROM_CENTER = 116
        const val BEARD_RADIUS = 12
        const val CACHE_CAPACITY = 16

        /** Last of the five in `underground_structures`, after the treasure, the two mineshafts and the trail ruins. */
        const val STRUCTURE_INDEX = 4
        const val UNDERGROUND_STRUCTURES_STEP = 3

        val GRID = StructurePlacementGrid(spacing = 34, separation = 12, salt = 94251327)
    }
}
