package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the flower forest, swamp and meadow flower patches. */
class BiomeFlowerOracle {
    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var flowers = 0
        for (y in 69..78) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (!Blocks.isAir(id) && id != Blocks.STONE.stateId && id != Blocks.GRASS_BLOCK.stateId) {
                        flowers++
                    }
                }
            }
        }
        return checksum to flowers
    }

    private fun report(
        name: String,
        body: (Chunk, WorldgenRandom, DecorationPos) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var flowers = 0
        val distinct = mutableSetOf<Int>()
        for (index in 0 until 60) {
            val chunk = groundChunk(index * 3 - 30, index * -5 + 10)
            val random = WorldgenRandom(LegacyRandom(9753L + index * 6364136223846793005L))
            body(chunk, random, DecorationPos(chunk.chunkX * 16 + 8, 71, chunk.chunkZ * 16 + 8))
            val (chunkChecksum, chunkFlowers) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            flowers += chunkFlowers
            for (y in 69..78) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        val id = chunk.getBlockStateId(x, y, z)
                        if (!Blocks.isAir(id) && id != Blocks.STONE.stateId && id != Blocks.GRASS_BLOCK.stateId) {
                            distinct.add(id)
                        }
                    }
                }
            }
        }
        println("ORACLE $name flowers=$flowers distinct=${distinct.size} checksum=$checksum")
    }

    @Test
    fun dump() {
        report("flowerForest") { c, r, o -> FlowerForestDecoration.placeAt(c, r, o) }
        report("swamp") { c, r, o -> SwampDecoration.placeAt(c, r, o) }
        report("meadow") { c, r, o -> MeadowDecoration.placeAt(c, r, o) }

        // The noise selectors on their own, so the list order and the mapping are pinned directly.
        val forest = StringBuilder()
        val meadow = StringBuilder()
        for (i in 0 until 24) {
            val x = i * 137 - 500
            val y = 64 + i
            val z = i * -89 + 300
            forest.append(FlowerForestDecoration.flowerAtForTest(x, y, z)).append(',')
            meadow.append(MeadowDecoration.flowerStateAtForTest(DecorationPos(x, y, z))).append(',')
        }
        println("ORACLE forestPicks=$forest")
        println("ORACLE meadowPicks=$meadow")
    }
}
