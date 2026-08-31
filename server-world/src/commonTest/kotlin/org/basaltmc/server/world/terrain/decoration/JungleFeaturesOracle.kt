package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isVineState
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the jungle melon and vine features. */
class JungleFeaturesOracle {
    /**
     * Grass floor with a soft cover stripe, and stone pillars up through the vine height band: the
     * melon replaces more than air, and a vine needs a solid face beside open air.
     */
    private fun jungleChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, Blocks.GRASS_BLOCK.stateId)
                if (z % 4 == 0) chunk.setBlockStateId(x, 71, z, stateId("short_grass"))
                if ((x % 5 == 0 && z % 5 == 0)) {
                    for (y in 71..102) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var melons = 0
        var vines = 0
        for (y in 60..104) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("melon")) melons++
                    if (Blocks.isVineState(id)) vines++
                }
            }
        }
        return Triple(checksum, melons, vines)
    }

    /** [chunks] is per case: the sparse melon is 1/64, so it needs a far wider sample to fire. */
    private fun report(
        name: String,
        chunks: Int = 60,
        body: (Chunk, WorldgenRandom) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var melons = 0
        var vines = 0
        val distinct = HashSet<Int>()
        for (index in 0 until chunks) {
            val chunk = jungleChunk(index * 3 - 30, index * -5 + 10)
            val random = WorldgenRandom(LegacyRandom(717171L + index * 6364136223846793005L))
            body(chunk, random)
            val (chunkChecksum, chunkMelons, chunkVines) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            melons += chunkMelons
            vines += chunkVines
            for (y in 60..104) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        val id = chunk.getBlockStateId(x, y, z)
                        if (Blocks.isVineState(id)) distinct.add(id)
                    }
                }
            }
        }
        println("ORACLE $name melons=$melons vines=$vines distinct=${distinct.size} checksum=$checksum")
    }

    @Test
    fun dump() {
        report("melonNormal") { c, r -> JungleMelonDecoration.NORMAL.decorate(c, r) }
        report("melonSparse", chunks = 400) { c, r -> JungleMelonDecoration.SPARSE.decorate(c, r) }
        report("vines") { c, r -> JungleVineDecoration.decorate(c, r) }

        // One vine per face, so the five packed states and the direction priority are pinned.
        val states = StringBuilder()
        for (face in 0 until 6) {
            val chunk = Chunk(0, 0)
            val dx =
                when (face) {
                    3 -> -1
                    4 -> 1
                    else -> 0
                }
            val dy = if (face == 0) 1 else 0
            val dz =
                when (face) {
                    1 -> -1
                    2 -> 1
                    else -> 0
                }
            if (face != 5) chunk.setBlockStateId(8 + dx, 80 + dy, 8 + dz, Blocks.STONE.stateId)
            val ok = JungleVineDecoration.placeAt(chunk, DecorationPos(8, 80, 8))
            states.append("$ok/${chunk.getBlockStateId(8, 80, 8)},")
        }
        println("ORACLE faces=$states")
    }
}
