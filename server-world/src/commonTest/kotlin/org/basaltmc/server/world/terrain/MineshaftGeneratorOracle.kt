package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for where mineshafts start and what they write. */
class MineshaftGeneratorOracle {
    private fun stoneChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -16..48) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -16..52) {
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

    private fun report(
        name: String,
        seed: Long,
        chunks: Int,
        biomeAt: (Int, Int, Int) -> String,
    ) {
        val generator =
            MineshaftStructureGenerator(
                biomeAt,
                // savanna is in both tags, so which type the coin flip drew is what decides.
                normalBiomes = setOf("minecraft:plains", "minecraft:forest", "minecraft:savanna"),
                mesaBiomes = setOf("minecraft:badlands", "minecraft:savanna"),
            )
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var entities = 0
        for (index in 0 until chunks) {
            val chunkX = index % 8 - 4
            val chunkZ = index / 8 - 4
            val chunk = stoneChunk(chunkX, chunkZ)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkBlockEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            blockEntities += chunkBlockEntities
            entities += chunk.generatedEntities().size
        }
        println(
            "ORACLE $name written=$written blockEntities=$blockEntities entities=$entities checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("plains", 12345L, 64) { _, _, _ -> "minecraft:plains" }
        // The mesa entry has its own feature seed, so the same chunks come out differently.
        report("badlands", 12345L, 64) { _, _, _ -> "minecraft:badlands" }
        // A biome in neither tag leaves the chunks untouched.
        report("nether", 12345L, 64) { _, _, _ -> "minecraft:nether_wastes" }
        report("otherSeed", 7L, 64) { _, _, _ -> "minecraft:plains" }
        // A biome in both tags, so the type comes from the coin flip and not from the refusal.
        report("both", 12345L, 64) { _, _, _ -> "minecraft:savanna" }
        // Layered by height, so which y the start probes at is observable.
        report("layered", 12345L, 64) { _, y, _ -> if (y >= 32) "minecraft:plains" else "minecraft:badlands" }
    }
}
