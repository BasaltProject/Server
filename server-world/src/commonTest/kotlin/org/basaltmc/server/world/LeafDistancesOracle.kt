package org.basaltmc.server.world

import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the chunk-wide leaf distance pass. */
class LeafDistancesOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * A canopy over two trunks with a detached clump beside it: the detached leaves keep the decay
     * distance because nothing sourced them, and the two trunks meet in the middle of the canopy.
     */
    private fun canopyChunk(
        chunkX: Int,
        chunkZ: Int,
        kind: String,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        val log = stateId("${kind}_log")
        val leaves = stateId("${kind}_leaves")
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, 60, z, stateId("dirt"))
            }
        }
        for (trunk in listOf(4 to 4, 11 to 10)) {
            for (y in 61..68) chunk.setBlockStateId(trunk.first, y, trunk.second, log)
        }
        for (x in 1 until 15) {
            for (z in 1 until 15) {
                for (y in 66..70) {
                    if (chunk.getBlockStateId(x, y, z) == log) continue
                    chunk.setBlockStateId(x, y, z, leaves)
                }
            }
        }
        // A short tree whose whole trunk sits in the section below its leaves: without the extra
        // section either way, the walk would start above the only thing sourcing this canopy.
        for (y in 61..63) chunk.setBlockStateId(14, y, 2, log)
        for (y in 64..66) {
            for (x in 13 until 16) {
                for (z in 1 until 4) {
                    if (chunk.getBlockStateId(x, y, z) == log) continue
                    chunk.setBlockStateId(x, y, z, leaves)
                }
            }
        }
        // A clump with no trunk under it, far enough that the walk never reaches it.
        for (y in 74..76) {
            for (x in 0 until 3) {
                for (z in 13 until 16) chunk.setBlockStateId(x, y, z, leaves)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var leaves = 0
        for (y in 55..80) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks
                            .byStateId(id)
                            ?.block
                            ?.path
                            ?.endsWith("_leaves") == true
                    ) {
                        leaves++
                    }
                }
            }
        }
        return checksum to leaves
    }

    @Test
    fun dump() {
        for (kind in listOf("oak", "spruce", "mangrove")) {
            val chunk = canopyChunk(0, 0, kind)
            val corrected = LeafDistances.assign(chunk)
            val (checksum, leaves) = fold(chunk)
            println("ORACLE $kind corrected=$corrected leaves=$leaves checksum=$checksum")
            // A second pass has nothing left to correct: the first one is a fixed point.
            val again = LeafDistances.assign(chunk)
            println("ORACLE ${kind}_again corrected=$again leaves=$leaves checksum=${fold(chunk).first}")
        }
        val bare = Chunk(1, -1)
        for (x in 0 until 16) for (z in 0 until 16) bare.setBlockStateId(x, 60, z, stateId("stone"))
        val (bareChecksum, bareLeaves) = fold(bare)
        println("ORACLE bare corrected=${LeafDistances.assign(bare)} leaves=$bareLeaves checksum=$bareChecksum")
    }
}
