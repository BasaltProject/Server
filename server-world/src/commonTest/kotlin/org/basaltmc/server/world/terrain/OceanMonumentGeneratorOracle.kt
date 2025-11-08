package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for where ocean monuments stand and what they write. */
class OceanMonumentGeneratorOracle {
    private fun seaChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -16..35) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in 36..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -16..70) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId && id != Blocks.WATER.stateId && id != Blocks.AIR.stateId) {
                        written++
                    }
                }
            }
        }
        return Triple(checksum, written, chunk.generatedEntities().size)
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
        chunks: Int,
    ) {
        val generator =
            OceanMonumentStructureGenerator(
                noiseBiomeAt = { _, _, _ -> biome },
                monumentBiomes = setOf("minecraft:deep_ocean", "minecraft:deep_cold_ocean"),
                surroundingBiomes = setOf("minecraft:deep_ocean", "minecraft:deep_cold_ocean", "minecraft:ocean"),
                oceanFloorAt = { _, _ -> 36 },
            )
        // Centred on the candidate the grid picks for the middle cell, which is 32 chunks wide.
        val centre = OceanMonumentStructureGenerator.GRID.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var entities = 0
        var starts = 0
        for (index in 0 until chunks) {
            val chunkX = centre.first + index % 12 - 6
            val chunkZ = centre.second + index / 12 - 6
            val chunk = seaChunk(chunkX, chunkZ)
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
        report("deepOcean", 4711L, "minecraft:deep_ocean", 144)
        // A biome outside the monument tag refuses it even though the surroundings still pass.
        report("ocean", 4711L, "minecraft:ocean", 144)
        // A biome in neither tag stops the sweep at the first cell.
        report("plains", 4711L, "minecraft:plains", 144)
        report("otherSeed", 8L, "minecraft:deep_ocean", 144)
    }
}
