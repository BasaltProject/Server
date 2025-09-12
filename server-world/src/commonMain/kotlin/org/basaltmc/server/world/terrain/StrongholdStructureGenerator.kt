package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions
import org.basaltmc.server.world.structure.ConcentricRingsPlacement
import org.basaltmc.server.world.structure.StrongholdBrush
import org.basaltmc.server.world.structure.StrongholdCorridors
import org.basaltmc.server.world.structure.StrongholdLayout
import org.basaltmc.server.world.structure.StrongholdLibrary
import org.basaltmc.server.world.structure.StrongholdPiece
import org.basaltmc.server.world.structure.StrongholdPieceKind
import org.basaltmc.server.world.structure.StrongholdPortalRoom
import org.basaltmc.server.world.structure.StrongholdRooms
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructurePieceFill
import org.basaltmc.server.world.structure.StructureStartRecord
import kotlin.concurrent.AtomicReference

/**
 * The stronghold. // parity: StrongholdStructure. Unlike every other structure here it is not spread
 * over a grid: the world holds 128 fixed chunks laid out on rings ([ConcentricRingsPlacement]), and
 * the maze itself reaches 112 blocks out from the chunk that starts it.
 */
public class StrongholdStructureGenerator(
    private val rings: ConcentricRingsPlacement,
    /**
     * The world seed. The ring list is the same for every chunk, so it is held here rather than
     * recomputed from the one `generateInto` is handed: building it is 128 biome searches of 57 by
     * 57 cells each, and doing that once per generating thread cost minutes of startup.
     */
    private val seed: Long,
    private val noiseBiomeAt: (quartX: Int, quartY: Int, quartZ: Int) -> String,
    private val overworldBiomes: Set<String>,
    private val seaLevel: Int = 63,
) {
    // parity: the order matters for `locate`, which keeps the *first* of two equally near rings.
    private val ringPositions: List<Pair<Int, Int>> by lazy { rings.ringPositions(seed) }
    private val ringChunks: Set<Long> by lazy { ringPositions.map { key(it.first, it.second) }.toSet() }
    private val layouts = AtomicReference<Map<Long, List<StrongholdPiece>>>(emptyMap())

    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val clip = chunkClip(chunk.chunkX, chunk.chunkZ)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        var seeded = false
        for ((startX, startZ) in nearbyStarts(chunk.chunkX, chunk.chunkZ)) {
            val pieces = layoutFor(startX, startZ, seed)
            val here = pieces.filter { it.box.intersects(clip) }
            if (here.isEmpty()) continue
            if (!seeded) {
                val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
                random.setFeatureSeed(decorationSeed, STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)
                seeded = true
            }
            for (piece in here) draw(chunk, piece, clip, random)
            chunk.structures.add(
                StructureStartRecord(NAME, startX, startZ, wholeBox(pieces), here.map { it.box }),
            )
        }
    }

    /**
     * The stronghold nearest to a block position, or null in a world with none. // parity:
     * ChunkGenerator.getNearestGeneratedStructure walks the whole ring list keeping the smallest squared
     * distance from each ring chunk's centre at y 32, but reports that chunk's minimum corner.
     */
    public fun locate(
        fromX: Int,
        fromZ: Int,
    ): Pair<Int, Int>? {
        var best: Pair<Int, Int>? = null
        var bestDistance = Double.MAX_VALUE
        for ((chunkX, chunkZ) in ringPositions) {
            val dx = (chunkX * 16 + 8).toDouble() - fromX
            val dz = (chunkZ * 16 + 8).toDouble() - fromZ
            val distance = dx * dx + dz * dz
            if (best != null && distance >= bestDistance) continue
            best = chunkX to chunkZ
            bestDistance = distance
        }
        return best
    }

    /** // parity: `terrain_adaptation: bury`, which hollows the rock the whole maze sits in. */
    public fun beardified(
        base: DensityFunction,
        chunkX: Int,
        chunkZ: Int,
        seed: Long,
    ): DensityFunction {
        val nearClip = chunkClip(chunkX, chunkZ, BEARD_RADIUS)
        val rigids = mutableListOf<StructureBeardifier.Rigid>()
        for ((startX, startZ) in nearbyStarts(chunkX, chunkZ)) {
            for (piece in layoutFor(startX, startZ, seed)) {
                if (!piece.box.intersects(nearClip)) continue
                rigids += StructureBeardifier.Rigid(piece.box, 0, TerrainAdjustment.BURY)
            }
        }
        if (rigids.isEmpty()) return base
        return DensityFunctions.add(base, StructureBeardifier(rigids))
    }

    /** The ring chunks whose maze can still reach ([chunkX], [chunkZ]). */
    private fun nearbyStarts(
        chunkX: Int,
        chunkZ: Int,
    ): List<Pair<Int, Int>> {
        val chunks = ringChunks
        if (chunks.isEmpty()) return emptyList()
        val found = mutableListOf<Pair<Int, Int>>()
        for (dx in -REACH_CHUNKS..REACH_CHUNKS) {
            for (dz in -REACH_CHUNKS..REACH_CHUNKS) {
                val startX = chunkX + dx
                val startZ = chunkZ + dz
                if (key(startX, startZ) !in chunks) continue
                if (!validBiome(startX, startZ)) continue
                found += startX to startZ
            }
        }
        return found
    }

    /**
     * // parity: Structure.isValidBiome, sampled at the chunk's own corner and at y 0. The rings
     * already looked for a biome the stronghold likes; this only refuses one that is not Overworld
     * at all.
     */
    private fun validBiome(
        chunkX: Int,
        chunkZ: Int,
    ): Boolean = noiseBiomeAt((chunkX * 16) shr 2, 0, (chunkZ * 16) shr 2) in overworldBiomes

    private fun layoutFor(
        startX: Int,
        startZ: Int,
        seed: Long,
    ): List<StrongholdPiece> {
        val key = key(startX, startZ)
        layouts.value[key]?.let { return it }
        val random = WorldgenRandom(LegacyRandom(0L))
        val built =
            StrongholdLayout(seaLevel, WorldHeight.MIN_Y).build(
                random,
                reseed = { attempt -> random.setLargeFeatureSeed(seed + attempt, startX, startZ) },
                chunkX = startX,
                chunkZ = startZ,
            )
        while (true) {
            val current = layouts.value
            current[key]?.let { return it }
            if (layouts.compareAndSet(current, current + (key to built))) return built
        }
    }

    private fun draw(
        chunk: Chunk,
        piece: StrongholdPiece,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        val fill = StructurePieceFill(piece.box, piece.orientation, chunk, clip)
        val brush = StrongholdBrush(fill, chunk, random)
        when (piece.kind) {
            StrongholdPieceKind.START, StrongholdPieceKind.STAIRS_DOWN -> StrongholdCorridors.stairsDown(brush, piece)
            StrongholdPieceKind.STRAIGHT -> StrongholdCorridors.straight(brush, piece)
            StrongholdPieceKind.STRAIGHT_STAIRS_DOWN -> StrongholdCorridors.straightStairsDown(brush, piece)
            StrongholdPieceKind.LEFT_TURN, StrongholdPieceKind.RIGHT_TURN -> StrongholdCorridors.turn(brush, piece)
            StrongholdPieceKind.CHEST_CORRIDOR -> StrongholdCorridors.chestCorridor(brush, piece)
            StrongholdPieceKind.FILLER_CORRIDOR -> StrongholdCorridors.fillerCorridor(brush, piece)
            StrongholdPieceKind.PRISON_HALL -> StrongholdRooms.prisonHall(brush, piece)
            StrongholdPieceKind.ROOM_CROSSING -> StrongholdRooms.roomCrossing(brush, piece)
            StrongholdPieceKind.FIVE_CROSSING -> StrongholdRooms.fiveCrossing(brush, piece)
            StrongholdPieceKind.LIBRARY -> StrongholdLibrary.place(brush, piece)
            StrongholdPieceKind.PORTAL_ROOM -> StrongholdPortalRoom.place(brush)
        }
    }

    private fun wholeBox(pieces: List<StrongholdPiece>): StructureBoundingBox {
        var box = pieces.first().box
        for (piece in pieces) {
            box =
                StructureBoundingBox(
                    minOf(box.minX, piece.box.minX),
                    minOf(box.minY, piece.box.minY),
                    minOf(box.minZ, piece.box.minZ),
                    maxOf(box.maxX, piece.box.maxX),
                    maxOf(box.maxY, piece.box.maxY),
                    maxOf(box.maxZ, piece.box.maxZ),
                )
        }
        return box
    }

    private fun chunkClip(
        chunkX: Int,
        chunkZ: Int,
        margin: Int = 0,
    ): StructureBoundingBox =
        StructureBoundingBox(
            chunkX * 16 - margin,
            WorldHeight.MIN_Y,
            chunkZ * 16 - margin,
            chunkX * 16 + 15 + margin,
            WorldHeight.MAX_Y,
            chunkZ * 16 + 15 + margin,
        )

    private fun key(
        chunkX: Int,
        chunkZ: Int,
    ): Long = (chunkX.toLong() shl 32) or (chunkZ.toLong() and 0xFFFFFFFFL)

    public companion object {
        public const val NAME: String = "stronghold"

        /** Twentieth of the 26 structures of `surface_structures`, in registry order. */
        public const val STRUCTURE_INDEX: Int = 19

        private const val SURFACE_STRUCTURES_STEP = 4
        private const val BEARD_RADIUS = 12

        /** A maze reaches 112 blocks out from the chunk it starts in, plus the last room's own size. */
        private const val REACH_CHUNKS = 9
    }
}
