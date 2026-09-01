package org.basaltmc.server.world

import kotlin.test.Test

/** The frozen Kotlin as the oracle for the two world types that are not the vanilla pipeline. */
class WorldTypesOracle {
    private fun report(
        name: String,
        chunk: Chunk,
    ) {
        var checksum = -3750763034362895579L
        var solid = 0
        for (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val state = chunk.getBlockStateId(x, y, z)
                    if (state != 0) {
                        solid++
                        checksum = (checksum xor x.toLong()) * 1099511628211L
                        checksum = (checksum xor y.toLong()) * 1099511628211L
                        checksum = (checksum xor z.toLong()) * 1099511628211L
                        checksum = (checksum xor state.toLong()) * 1099511628211L
                    }
                }
            }
        }
        val heights = chunk.heightmap().joinToString("") { it.toString() + "," }
        var heightChecksum = -3750763034362895579L
        for (char in heights) heightChecksum = (heightChecksum xor char.code.toLong()) * 1099511628211L
        println("ORACLE $name solid=$solid checksum=$checksum heights=$heightChecksum")
    }

    @Test
    fun dump() {
        report("flat", FlatWorldGenerator.generate(0, 0))
        report("flatFar", FlatWorldGenerator.generate(-13, 7))
        report("flatNether", FlatWorldGenerator.generate(0, 0, ChunkHeight.NETHER))
        report("void", VoidWorldGenerator.generate(0, 0))
        // The platform straddles the chunk edge, so the far side of it lands in the next chunk over.
        report("voidEdge", VoidWorldGenerator.generate(-1, -1, spawnX = 0, spawnZ = 0))
        report("voidNeighbour", VoidWorldGenerator.generate(1, 0))
        report("voidWide", VoidWorldGenerator.generate(0, 0, platformRadius = 20))
        report("voidBare", VoidWorldGenerator.generate(0, 0, platformRadius = 0))
        report("voidDeep", VoidWorldGenerator.generate(0, 0, spawnY = -60))
    }
}
