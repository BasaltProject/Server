package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the mineshaft block filling. */
class MineshaftFillOracle {
    /** Stone with a water pocket well away from the shaft, so the invalid-location scan can bite. */
    private fun stoneChunk(
        chunkX: Int,
        chunkZ: Int,
        flooded: Boolean,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -8..60) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                if (flooded) chunk.setBlockStateId(x, 30, z, Blocks.WATER.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in -8..64) {
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
        type: MineshaftType,
        flooded: Boolean,
        biome: String,
        shafts: Int,
    ) {
        var checksum = -3750763034362895579L
        var written = 0
        var blockEntities = 0
        var entities = 0
        var deferred = 0
        var placed = 0
        for (index in 0 until shafts) {
            val chunkX = index * 7 - 20
            val chunkZ = index * -3 + 11
            val pieces = MineshaftLayout.assemble(seed + index, chunkX, chunkZ, type, 63, -64)
            val chunk = stoneChunk(chunkX, chunkZ, flooded)
            val clip =
                StructureBoundingBox(chunkX * 16, -8, chunkZ * 16, chunkX * 16 + 15, 64, chunkZ * 16 + 15)
            val blocks = MineshaftBlocks(type)
            val random = WorldgenRandom(LegacyRandom(seed + index))
            for (piece in pieces) {
                if (!piece.box.intersects(clip)) continue
                placed++
                MineshaftFill.place(piece, blocks, chunk, clip, random) { _, _, _ -> biome }
            }
            val (chunkChecksum, chunkWritten, chunkBlockEntities) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            blockEntities += chunkBlockEntities
            entities += chunk.generatedEntities().size
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println(
            "ORACLE $name placed=$placed written=$written blockEntities=$blockEntities entities=$entities " +
                "deferred=$deferred checksum=$checksum",
        )
    }

    @Test
    fun dump() {
        report("normal", 4242L, MineshaftType.NORMAL, flooded = false, biome = "minecraft:plains", shafts = 40)
        // Dark oak instead of oak, and a shaft that never sank below sea level.
        report("mesa", 4242L, MineshaftType.MESA, flooded = false, biome = "minecraft:badlands", shafts = 40)
        // Water anywhere in the shell makes a piece refuse to write itself.
        report("flooded", 4242L, MineshaftType.NORMAL, flooded = true, biome = "minecraft:plains", shafts = 40)
        // The one biome the shaft is not allowed to exist in.
        report("deepDark", 4242L, MineshaftType.NORMAL, flooded = false, biome = "minecraft:deep_dark", shafts = 40)
    }
}
