package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.ConcentricRingsPlacement
import kotlin.test.Test

/** The frozen Kotlin as the oracle for where strongholds sit and what they write. */
class StrongholdGeneratorOracle {
    private fun stoneChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -32..70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -32..74) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.STONE.stateId && id != Blocks.AIR.stateId) written++
                }
            }
        }
        return Triple(checksum, written, chunk.blockEntities().size)
    }

    private fun generator(
        seed: Long,
        biome: String,
    ): StrongholdStructureGenerator {
        val rings =
            ConcentricRingsPlacement(
                distance = 2,
                spread = 1,
                count = 4,
                preferredBiomes = setOf(biome),
                noiseBiomeAt = { _, _, _ -> biome },
            )
        return StrongholdStructureGenerator(rings, seed, { _, _, _ -> biome }, setOf(biome))
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
        chunks: Int,
    ) {
        val generator = generator(seed, biome)
        // Centred on the ring the world actually placed, so the window sees the maze at all.
        val centre = generator.locate(0, 0) ?: error("no stronghold")
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var starts = 0
        for (index in 0 until chunks) {
            val chunkX = centre.first + index % 10 - 5
            val chunkZ = centre.second + index / 10 - 5
            val chunk = stoneChunk(chunkX, chunkZ)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkBlockEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            blockEntities += chunkBlockEntities
            starts += chunk.structures.starts().size
        }
        val located = generator.locate(0, 0)
        println(
            "ORACLE $name written=$written blockEntities=$blockEntities starts=$starts " +
                "located=${located?.first},${located?.second} checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("plains", 4242L, "minecraft:plains", 100)
        // A biome the rings do not accept leaves the world without a stronghold at all.
        report("otherSeed", 99L, "minecraft:plains", 100)
    }
}
