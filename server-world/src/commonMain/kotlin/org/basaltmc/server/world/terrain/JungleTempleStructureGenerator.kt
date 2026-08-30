package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructureStartRecord
import kotlin.math.abs

/**
 * The jungle temple. // parity: JungleTempleStructure, one piece twelve by fifteen, so the chunks it
 * reaches into place their own part of it.
 */
public class JungleTempleStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val templeBiomes: Set<String>,
    private val surfaceHeightAt: (x: Int, z: Int) -> Int,
) {
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val cellX = chunk.chunkX.floorDiv(GRID.spacing)
        val cellZ = chunk.chunkZ.floorDiv(GRID.spacing)
        for (offsetX in -1..1) {
            for (offsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    GRID.candidateChunk(seed, (cellX + offsetX) * GRID.spacing, (cellZ + offsetZ) * GRID.spacing)
                if (abs(candidateX - chunk.chunkX) > REACH_CHUNKS || abs(candidateZ - chunk.chunkZ) > REACH_CHUNKS) {
                    continue
                }
                placeFrom(chunk, seed, candidateX, candidateZ)
            }
        }
    }

    private fun placeFrom(
        chunk: Chunk,
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ) {
        if (!GRID.isStructureChunk(seed, chunkX, chunkZ)) return

        val startRandom = WorldgenRandom(LegacyRandom(0L))
        startRandom.setLargeFeatureSeed(seed, chunkX, chunkZ)

        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        val stubY = surfaceHeightAt(centreX, centreZ) - 1
        if (biomeAt(centreX, stubY, centreZ) !in templeBiomes) return

        val orientation = HORIZONTAL[startRandom.nextInt(HORIZONTAL.size)]
        val piece = JungleTemplePiece(chunkX * 16, chunkZ * 16, orientation)
        if (!piece.settleOnGround { x, z -> surfaceHeightAt(x, z) }) return

        val clip = chunkClip(chunk.chunkX, chunk.chunkZ)
        if (!piece.bounds.intersects(clip)) return

        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)

        piece.place(chunk, clip, random)
        chunk.structures.add(
            StructureStartRecord(NAME, chunkX, chunkZ, piece.bounds, listOf(piece.bounds)),
        )
    }

    private fun chunkClip(
        chunkX: Int,
        chunkZ: Int,
    ): StructureBoundingBox =
        StructureBoundingBox(
            chunkX * 16,
            WorldHeight.MIN_Y,
            chunkZ * 16,
            chunkX * 16 + 15,
            WorldHeight.MAX_Y,
            chunkZ * 16 + 15,
        )

    public companion object {
        public const val NAME: String = "jungle_pyramid"

        /** Fifth of the 26 structures of `surface_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 4

        private const val SURFACE_STRUCTURES_STEP = 4
        private const val REACH_CHUNKS = 2

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(spacing = 32, separation = 8, salt = 14357619)

        /** // parity: Direction.Plane.HORIZONTAL, the array getRandomDirection draws from. */
        private val HORIZONTAL =
            listOf(
                StructureDirection.NORTH,
                StructureDirection.EAST,
                StructureDirection.SOUTH,
                StructureDirection.WEST,
            )
    }
}
