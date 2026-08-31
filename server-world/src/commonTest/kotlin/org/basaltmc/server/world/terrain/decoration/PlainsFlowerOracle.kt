package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isWildflowersState
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the plains flowers and the wildflower beds. */
class PlainsFlowerOracle {
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

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var flowers = 0
        var wild = 0
        for (y in 69..78) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.isWildflowersState(id)) {
                        wild++
                    } else if (!Blocks.isAir(id) && id != Blocks.STONE.stateId && id != Blocks.GRASS_BLOCK.stateId) {
                        flowers++
                    }
                }
            }
        }
        return Triple(checksum, flowers, wild)
    }

    private fun report(
        name: String,
        body: (Chunk, WorldgenRandom, DecorationPos) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var flowers = 0
        var wild = 0
        val distinct = mutableSetOf<Int>()
        for (index in 0 until 60) {
            val chunk = groundChunk(index * 3 - 30, index * -5 + 10)
            val random = WorldgenRandom(LegacyRandom(1357L + index * 6364136223846793005L))
            body(chunk, random, DecorationPos(chunk.chunkX * 16 + 8, 71, chunk.chunkZ * 16 + 8))
            val (chunkChecksum, chunkFlowers, chunkWild) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            flowers += chunkFlowers
            wild += chunkWild
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
        println("ORACLE $name flowers=$flowers wild=$wild distinct=${distinct.size} checksum=$checksum")
    }

    @Test
    fun dump() {
        report("plainsFlowers") { c, r, o -> PlainsDecoration.placeFlowerPlainAt(c, r, o.x, o.y, o.z) }
        report("wildflowersBirch") { c, r, o -> ForestFloorDecoration.placeWildflowersAt(c, r, o, 64) }
        report("wildflowersMeadow") { c, r, o -> ForestFloorDecoration.placeWildflowersAt(c, r, o, 8) }

        // The plains selector on its own: it reads a noise then draws, so the sequence pins both.
        val picks = StringBuilder()
        val random = WorldgenRandom(LegacyRandom(555L))
        for (i in 0 until 40) {
            val x = i * 211 - 4000
            val z = i * -173 + 3000
            picks.append(PlainsDecoration.flowerStateIdForTest(random, DecorationPos(x, 71, z))).append(',')
        }
        println("ORACLE plainsPicks=$picks after=" + random.nextInt())

        val beds = StringBuilder()
        for (choice in 0 until 16) {
            beds.append(ForestFloorDecoration.flowerBedStateIdForTest(stateId("wildflowers"), choice)).append(',')
        }
        println("ORACLE beds=$beds")
    }
}
