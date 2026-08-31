package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isPinkPetalsState
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the pale garden and cherry petal features. */
class FlowerFeaturesOracle {
    /** Grass with a leaf canopy over half of it: the pale garden patch uses the no-leaves heightmap. */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, Blocks.GRASS_BLOCK.stateId)
                if (x < 8) chunk.setBlockStateId(x, 76, z, Blocks.OAK_LEAVES.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var eyeblossom = 0
        var petals = 0
        for (y in 69..80) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("closed_eyeblossom")) eyeblossom++
                    if (Blocks.isPinkPetalsState(id)) petals++
                }
            }
        }
        return Triple(checksum, eyeblossom, petals)
    }

    private fun report(name: String, body: (Chunk, WorldgenRandom, DecorationPos) -> Unit) {
        var checksum = -3750763034362895579L
        var eyeblossom = 0
        var petals = 0
        var ticks = 0
        for (index in 0 until 60) {
            val chunk = groundChunk(index * 3 - 30, index * -5 + 10)
            val random = WorldgenRandom(LegacyRandom(4321L + index * 6364136223846793005L))
            body(chunk, random, DecorationPos(chunk.chunkX * 16 + 8, 71, chunk.chunkZ * 16 + 8))
            val (chunkChecksum, chunkEyes, chunkPetals) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            eyeblossom += chunkEyes
            petals += chunkPetals
            ticks += chunk.scheduledBlockTicks().size
        }
        println("ORACLE $name eyeblossom=$eyeblossom petals=$petals ticks=$ticks checksum=$checksum")
    }

    @Test
    fun dump() {
        report("paleFlowerPatch") { c, r, _ -> PaleGardenDecoration.FLOWER_PATCH.decorate(c, r) }
        report("paleRareFlower") { c, r, _ -> PaleGardenDecoration.RARE_FLOWER.decorate(c, r) }
        report("paleAt") { c, r, o -> PaleGardenDecoration.placeAt(c, r, o) }
        report("paleSingleAt") { c, r, o -> PaleGardenDecoration.placeSingleAt(c, r, o) }
        report("cherryFlowers") { c, r, _ -> CherryGroveDecoration.FLOWERS_ONLY.decorate(c, r) }
        report("cherryAt") { c, r, o -> CherryGroveDecoration.placeAt(c, r, o) }

        // Every distinct petal state the patch actually produced, so the amount and facing packing
        // is pinned by what lands rather than by reading the private helper.
        val seen = mutableSetOf<Int>()
        for (index in 0 until 60) {
            val chunk = groundChunk(index * 3 - 30, index * -5 + 10)
            val random = WorldgenRandom(LegacyRandom(4321L + index * 6364136223846793005L))
            CherryGroveDecoration.placeAt(chunk, random,
                DecorationPos(chunk.chunkX * 16 + 8, 71, chunk.chunkZ * 16 + 8))
            for (y in 69..80) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        val id = chunk.getBlockStateId(x, y, z)
                        if (Blocks.isPinkPetalsState(id)) seen.add(id)
                    }
                }
            }
        }
        println("ORACLE petalStates=" + seen.sorted().joinToString(","))
    }
}
