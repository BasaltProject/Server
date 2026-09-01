package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the swamp hut on its stilts. */
class SwampHutGeneratorOracle {
    private val leaves =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("oak_leaves"),
            )?.stateId
            ?: error("unknown block 'oak_leaves'")

    /** Stone under water, so the stilts have to pour down through the swamp to reach the bed. */
    private fun swampChunk(
        chunkX: Int,
        chunkZ: Int,
        slope: Boolean,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                // An uneven bed, so the average the piece settles on is not the bed itself.
                val top = if (slope) 50 + (x + z).mod(6) else 52
                for (y in 30..top) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in top + 1..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                // A leaf canopy over the swamp: the average the hut settles on reads the heightmap
                // that ignores leaves, so a canopy must not move it.
                for (y in 64..65) chunk.setBlockStateId(x, y, z, leaves)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 28..90) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId &&
                        id != Blocks.WATER.stateId &&
                        id != leaves &&
                        id != Blocks.AIR.stateId
                    ) {
                        written++
                    }
                }
            }
        }
        // The residents' own position, not just how many there are.
        for (entity in chunk.generatedEntities()) {
            for (value in listOf(entity.x, entity.y, entity.z)) {
                checksum = (checksum xor value.toRawBits()) * 1099511628211L
            }
        }
        return Triple(checksum, written, chunk.generatedEntities().size)
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
        slope: Boolean,
        chunks: Int,
    ) {
        val generator =
            SwampHutStructureGenerator(
                biomeAt = { _, _, _ -> biome },
                surfaceHeightAt = { _, _ -> 63 },
            )
        val centre = SwampHutStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var entities = 0
        var starts = 0
        for (index in 0 until chunks) {
            val chunkX = centre.first + index % 6 - 3
            val chunkZ = centre.second + index / 6 - 3
            val chunk = swampChunk(chunkX, chunkZ, slope)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            entities += chunkEntities
            starts += chunk.structures.starts().size
        }
        println("ORACLE $name written=$written entities=$entities starts=$starts checksum=$checksum")
    }

    @Test
    fun dump() {
        report("swamp", 8080L, "minecraft:swamp", slope = true, chunks = 36)
        // A flat bed, so the average the hut settles on is the bed itself.
        report("flat", 8080L, "minecraft:swamp", slope = false, chunks = 36)
        // Any other biome leaves the chunks alone.
        report("plains", 8080L, "minecraft:plains", slope = true, chunks = 36)
        report("otherSeed", 21L, "minecraft:swamp", slope = true, chunks = 36)
        report("thirdSeed", 44L, "minecraft:swamp", slope = true, chunks = 36)
    }
}
