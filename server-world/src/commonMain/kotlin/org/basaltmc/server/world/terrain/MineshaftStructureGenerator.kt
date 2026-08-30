package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.MineshaftBlocks
import org.basaltmc.server.world.structure.MineshaftFill
import org.basaltmc.server.world.structure.MineshaftLayout
import org.basaltmc.server.world.structure.MineshaftPiece
import org.basaltmc.server.world.structure.MineshaftType
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructurePlacementGrid
import kotlin.concurrent.AtomicReference

/**
 * Mineshafts, from which chunks start one to which blocks land in this chunk. // parity: the set is
 * `spacing 1`, `separation 0`, `frequency 0.004`, so the roll is what makes one rare. A shaft reaches
 * 80 blocks, so a chunk looks several chunks out for the starts that can touch it.
 */
public class MineshaftStructureGenerator(
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    /** The biomes of `#minecraft:has_structure/mineshaft`, read from the jar by the caller. */
    private val normalBiomes: Set<String>,
    /** The biomes of `#minecraft:has_structure/mineshaft_mesa`. */
    private val mesaBiomes: Set<String>,
    private val seaLevel: Int = SEA_LEVEL,
) {
    private val grid =
        StructurePlacementGrid(
            spacing = 1,
            separation = 0,
            salt = 0,
            frequency = FREQUENCY,
            frequencyReduction = StructurePlacementGrid.FrequencyReduction.LEGACY_TYPE_3,
        )

    // Assembling a shaft is expensive and every chunk it touches asks for the same one, so the
    // result is shared. Immutable map behind an atomic, like the village assemblies.
    private val assembled = AtomicReference<Map<Long, List<MineshaftPiece>>>(emptyMap())

    /** Writes into [chunk] whatever any nearby mineshaft puts there. */
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val clip =
            StructureBoundingBox(
                chunk.chunkX * 16,
                WorldHeight.MIN_Y,
                chunk.chunkZ * 16,
                chunk.chunkX * 16 + 15,
                WorldHeight.MAX_Y,
                chunk.chunkZ * 16 + 15,
            )
        // parity: the two mineshafts are separate entries of `underground_structures`, so each gets
        // its own feature seed and one random shared by every start of that entry in this chunk.
        // Seeding per start, or off `setLargeFeatureSeed`, puts every roll inside the shaft (rails,
        // cobwebs, torches) on a stream vanilla never uses.
        for ((index, wanted) in ENTRIES) {
            val random = WorldgenRandom(XoroshiroRandom(seed))
            val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
            random.setFeatureSeed(decorationSeed, index, UNDERGROUND_STRUCTURES_STEP)
            for (startX in chunk.chunkX - REACH_CHUNKS..chunk.chunkX + REACH_CHUNKS) {
                for (startZ in chunk.chunkZ - REACH_CHUNKS..chunk.chunkZ + REACH_CHUNKS) {
                    val type = typeAt(seed, startX, startZ) ?: continue
                    if (type != wanted) continue
                    val pieces = piecesOf(seed, startX, startZ, type)
                    val blocks = MineshaftBlocks(type)
                    for (piece in pieces) {
                        if (!piece.box.intersects(clip)) continue
                        MineshaftFill.place(piece, blocks, chunk, clip, random, biomeAt)
                    }
                }
            }
        }
    }

    /**
     * Which mineshaft, if any, starts at this chunk. // parity: ChunkGenerator picks by weight from
     * the set and retries the other entry when the first one's biome refuses it, so a chunk with a
     * badlands biome still gets a shaft when the normal one was drawn first and rejected.
     */
    private fun typeAt(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): MineshaftType? {
        if (!grid.isStructureChunk(seed, chunkX, chunkZ)) return null
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val first = if (random.nextInt(2) == 0) MineshaftType.NORMAL else MineshaftType.MESA
        val biome = biomeAt(chunkX * 16 + 8, seaLevel, chunkZ * 16 + 8)
        if (allows(first, biome)) return first
        val second = if (first == MineshaftType.NORMAL) MineshaftType.MESA else MineshaftType.NORMAL
        return second.takeIf { allows(it, biome) }
    }

    private fun allows(
        type: MineshaftType,
        biome: String,
    ): Boolean = biome in if (type == MineshaftType.MESA) mesaBiomes else normalBiomes

    private fun piecesOf(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
        type: MineshaftType,
    ): List<MineshaftPiece> {
        val key = (chunkX.toLong() shl 32) or (chunkZ.toLong() and 0xFFFFFFFFL)
        assembled.value[key]?.let { return it }
        val pieces = MineshaftLayout.assemble(seed, chunkX, chunkZ, type, seaLevel, WorldHeight.MIN_Y)
        while (true) {
            val current = assembled.value
            current[key]?.let { return it }
            val next = if (current.size >= CACHE_CAPACITY) mapOf(key to pieces) else current + (key to pieces)
            if (assembled.compareAndSet(current, next)) return pieces
        }
    }

    private companion object {
        const val FREQUENCY = 0.004f
        const val SEA_LEVEL = 63

        const val UNDERGROUND_STRUCTURES_STEP = 3

        /** Second and third of the five structures of `underground_structures`, in registry order. */
        val ENTRIES = listOf(1 to MineshaftType.NORMAL, 2 to MineshaftType.MESA)

        /** A shaft reaches 80 blocks from its room, so five chunks out covers every piece. */
        const val REACH_CHUNKS = 6
        const val CACHE_CAPACITY = 64
    }
}
