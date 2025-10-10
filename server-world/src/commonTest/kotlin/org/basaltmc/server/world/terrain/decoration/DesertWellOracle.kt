package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the desert well. */
class DesertWellOracle {
    /**
     * Sand over stone, with a hollow carved under one corner: the well refuses to build where any
     * column of its 5 by 5 footprint has two air blocks beneath, so a solid plain would never reach
     * that abort.
     */
    private fun sandChunk(
        chunkX: Int,
        chunkZ: Int,
        hollow: Boolean,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId("sand"))
            }
        }
        if (hollow) {
            for (x in 4..6) {
                for (z in 4..6) {
                    chunk.setBlockStateId(x, 69, z, Blocks.AIR.stateId)
                    chunk.setBlockStateId(x, 68, z, Blocks.AIR.stateId)
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var sandstone = 0
        var suspicious = 0
        for (y in 60..80) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("sandstone") || id == stateId("sandstone_slab")) sandstone++
                    if (id == stateId("suspicious_sand")) suspicious++
                }
            }
        }
        return Triple(checksum, sandstone, suspicious)
    }

    @Test
    fun dump() {
        for (hollow in listOf(false, true)) {
            var checksum = -3750763034362895579L
            var sandstone = 0
            var suspicious = 0
            var entities = 0
            val seeds = StringBuilder()
            for (index in 0 until 30) {
                val chunk = sandChunk(index * 3 - 15, index * -5 + 10, hollow)
                val random = WorldgenRandom(LegacyRandom(818181L + index * 6364136223846793005L))
                DesertWellDecoration.placeAt(chunk, random, 5, 70, 5)
                val (chunkChecksum, chunkSandstone, chunkSuspicious) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                sandstone += chunkSandstone
                suspicious += chunkSuspicious
                for (entity in chunk.blockEntities()) {
                    entities++
                    seeds.append(entity.data.getLong("LootTableSeed") ?: 0L).append(',')
                }
            }
            val name = if (hollow) "hollow" else "solid"
            println("ORACLE $name sandstone=$sandstone suspicious=$suspicious entities=$entities checksum=$checksum")
            println("ORACLE ${name}Seeds=$seeds")
        }
    }
}
