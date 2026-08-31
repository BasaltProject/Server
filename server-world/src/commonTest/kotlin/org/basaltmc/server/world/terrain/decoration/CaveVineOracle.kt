package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the cave vine columns. */
class CaveVineOracle {
    /**
     * A ceiling with air below it, and a floor part way down: the column grows downward until it
     * hits something, so an unbounded shaft would never exercise the truncation and a solid mass
     * would never let it start.
     */
    private fun shaftChunk(floorY: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -60 until floorY) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                for (y in 80 until 90) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var vines = 0
        val distinct = HashSet<Int>()
        for (y in -60..90) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    val path = Blocks.byStateId(id)?.block?.path
                    if (path == "cave_vines" || path == "cave_vines_plant") {
                        vines++
                        distinct.add(id)
                    }
                }
            }
        }
        return Triple(checksum, vines, distinct.size)
    }

    private fun report(name: String, inMoss: Boolean, floorY: Int) {
        var checksum = -3750763034362895579L
        var vines = 0
        var distinct = 0
        var placed = 0
        val heights = StringBuilder()
        for (index in 0 until 40) {
            val chunk = shaftChunk(floorY)
            val random = WorldgenRandom(LegacyRandom(383838L + index * 6364136223846793005L))
            if (CaveVineColumnFeature.placeForTest(chunk, random, DecorationPos(8, 79, 8), inMoss)) placed++
            val (chunkChecksum, chunkVines, chunkDistinct) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            vines += chunkVines
            distinct += chunkDistinct
            heights.append(chunkVines).append(',')
        }
        println("ORACLE $name placed=$placed vines=$vines distinct=$distinct checksum=$checksum")
        println("ORACLE ${name}Heights=$heights")
    }

    @Test
    fun dump() {
        report("cave", false, 40)
        report("moss", true, 40)
        // A high floor truncates the column, which is the prioritize-tip path.
        report("caveShort", false, 76)
    }
}
