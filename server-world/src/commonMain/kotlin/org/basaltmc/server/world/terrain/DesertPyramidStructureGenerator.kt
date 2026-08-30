package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureDirection
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureStartRecord
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.math.abs

/**
 * The desert pyramid. // parity: DesertPyramidStructure, one piece 21 blocks square, so every chunk
 * it reaches into places its own part of it.
 */
public class DesertPyramidStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
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

        // parity: Structure.GenerationContext's own random, which is what the orientation comes off.
        val startRandom = WorldgenRandom(LegacyRandom(0L))
        startRandom.setLargeFeatureSeed(seed, chunkX, chunkZ)

        val centreX = chunkX * 16 + 8
        val centreZ = chunkZ * 16 + 8
        val stubY = surfaceHeightAt(centreX, centreZ) - 1
        if (biomeAt(centreX, stubY, centreZ).substringAfter(':') != BIOME) return

        val orientation = HORIZONTAL[startRandom.nextInt(HORIZONTAL.size)]
        val piece = DesertPyramidPiece(chunkX * 16, chunkZ * 16, orientation, -groundOffset(seed, chunkX, chunkZ))
        if (!piece.settleOnGround { x, z -> surfaceHeightAt(x, z) }) return

        val clip = chunkClip(chunk.chunkX, chunk.chunkZ)
        if (!piece.bounds.intersects(clip)) return

        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)
        // parity: the ground offset is drawn once per placing chunk whether or not it is used.
        random.nextInt(GROUND_OFFSET_BOUND)

        piece.place(chunk, clip, seed, random)
        placeSuspiciousSand(chunk, clip, seed, piece)
        chunk.structures.add(
            StructureStartRecord(NAME, chunkX, chunkZ, piece.bounds, listOf(piece.bounds)),
        )
    }

    /**
     * // parity: `-random.nextInt(3)` off the placing chunk's own feature random, so in the jar the
     * height depends on which chunk reached the pyramid first. Basalt draws it from the chunk the
     * pyramid starts in instead: every chunk then settles it at the same Y, which is the point.
     */
    private fun groundOffset(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Int {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunkX * 16, chunkZ * 16)
        random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)
        return random.nextInt(GROUND_OFFSET_BOUND)
    }

    /**
     * // parity: DesertPyramidStructure.afterPlace. Five to seven of the candidates become
     * suspicious sand and the rest plain sand, drawn from a shuffle seeded off the world seed at
     * the piece's centre, so every chunk that runs this agrees on which ones they are.
     */
    private fun placeSuspiciousSand(
        chunk: Chunk,
        clip: StructureBoundingBox,
        seed: Long,
        piece: DesertPyramidPiece,
    ) {
        piece.collapsedRoofPos?.let { setSuspiciousSand(chunk, clip, it) }
        // parity: a SortedArraySet over Vec3i, which orders by Y, then Z, then X.
        val unique =
            piece.suspiciousSandPositions
                .distinct()
                .sortedWith(compareBy({ it.y }, { it.z }, { it.x }))
                .toMutableList()
        val centre =
            StructurePos(
                piece.bounds.minX + (piece.bounds.maxX - piece.bounds.minX + 1) / 2,
                piece.bounds.minY + (piece.bounds.maxY - piece.bounds.minY + 1) / 2,
                piece.bounds.minZ + (piece.bounds.maxZ - piece.bounds.minZ + 1) / 2,
            )
        val random = LegacyRandom(seed).forkPositional().at(centre.x, centre.y, centre.z)
        // parity: Util.shuffle, which walks from the end and swaps into the tail.
        for (i in unique.size downTo 2) {
            val swapTo = random.nextInt(i)
            val held = unique[i - 1]
            unique[i - 1] = unique[swapTo]
            unique[swapTo] = held
        }
        var remaining = minOf(unique.size, SUSPICIOUS_MIN + random.nextInt(SUSPICIOUS_SPREAD))
        for (position in unique) {
            if (remaining > 0) {
                remaining--
                setSuspiciousSand(chunk, clip, position)
            } else if (clip.isInside(position)) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, SAND)
            }
        }
    }

    private fun setSuspiciousSand(
        chunk: Chunk,
        clip: StructureBoundingBox,
        position: StructurePos,
    ) {
        if (!clip.isInside(position)) return
        if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
        chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, SUSPICIOUS_SAND)
        chunk.setBlockEntity(
            ChunkBlockEntity(
                position.x and 15,
                position.y,
                position.z and 15,
                Identifier.vanilla("brushable_block"),
                NbtCompound().apply {
                    putString("LootTable", "minecraft:archaeology/desert_pyramid")
                    putLong("LootTableSeed", asLong(position))
                    putCompound("components", NbtCompound())
                },
            ),
        )
    }

    /** // parity: BlockPos.asLong, which is what seeds a brushable block's loot. */
    private fun asLong(position: StructurePos): Long =
        (position.x.toLong() and X_MASK shl X_SHIFT) or
            (position.y.toLong() and Y_MASK) or
            (position.z.toLong() and Z_MASK shl Z_SHIFT)

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
        public const val NAME: String = "desert_pyramid"

        /** Second of the 26 structures of `surface_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 1

        private const val SURFACE_STRUCTURES_STEP = 4
        private const val BIOME = "desert"
        private const val REACH_CHUNKS = 2
        private const val GROUND_OFFSET_BOUND = 3
        private const val SUSPICIOUS_MIN = 5
        private const val SUSPICIOUS_SPREAD = 3

        private const val X_MASK = 0x3FFFFFFL
        private const val Z_MASK = 0x3FFFFFFL
        private const val Y_MASK = 0xFFFL
        private const val X_SHIFT = 38
        private const val Z_SHIFT = 12

        public val GRID: StructurePlacementGrid =
            StructurePlacementGrid(spacing = 32, separation = 8, salt = 14357617)

        /** // parity: Direction.Plane.HORIZONTAL, the array getRandomDirection draws from. */
        private val HORIZONTAL =
            listOf(
                StructureDirection.NORTH,
                StructureDirection.EAST,
                StructureDirection.SOUTH,
                StructureDirection.WEST,
            )

        private val SAND = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("sand"))?.stateId ?: Blocks.AIR.stateId
        private val SUSPICIOUS_SAND =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("suspicious_sand"))?.stateId ?: Blocks.AIR.stateId
    }
}
