package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the cherry grove, meadow, flower forest and forest floor. */
class BiomeFloraOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun groundChunk(chunkX: Int, chunkZ: Int, surface: String): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId(surface))
            }
        }
        return chunk
    }

    private fun checksumOf(chunk: Chunk): Long {
        var checksum = -3750763034362895579L
        for (y in 60..140) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    checksum = (checksum xor chunk.getBlockStateId(x, y, z).toLong()) * 1099511628211L
                }
            }
        }
        return checksum
    }

    private fun plants(chunk: Chunk): Int {
        var count = 0
        for (y in 60..140) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id != Blocks.AIR.stateId && id != Blocks.STONE.stateId &&
                        Blocks.byStateId(id)?.block?.path !in setOf("grass_block", "podzol")
                    ) {
                        count++
                    }
                }
            }
        }
        return count
    }

    private fun report(name: String, surface: String = "grass_block", count: Int = 8, body: (Chunk, WorldgenRandom) -> Unit) {
        var checksum = -3750763034362895579L
        var placed = 0
        var deferred = 0
        var trailing = 0
        for (index in 0 until count) {
            val chunk = groundChunk(index * 5 - 30, index * -3 + 12, surface)
            val random = WorldgenRandom(LegacyRandom(7000L + index * 977L))
            body(chunk, random)
            checksum = (checksum xor checksumOf(chunk)) * 1099511628211L
            placed += plants(chunk)
            deferred += chunk.takeGeneratedBlockWrites().size
            trailing = random.nextInt()
        }
        println("ORACLE $name placed=$placed deferred=$deferred checksum=$checksum after=$trailing")
    }

    @Test
    fun dump() {
        report("cherryGrove") { c, r -> CherryGroveDecoration.decorate(c, r) }
        report("cherryPetals") { c, r -> CherryGroveDecoration.FLOWERS_ONLY.decorate(c, r) }
        report("cherryPetalsAt") { c, r ->
            CherryGroveDecoration.placeAt(c, r, DecorationPos(c.chunkX * 16 + 8, 71, c.chunkZ * 16 + 8))
        }
        report("meadow") { c, r -> MeadowDecoration.decorate(c, r) }
        report("meadowWithoutTrees") { c, r -> MeadowDecoration.WITHOUT_TREES.decorate(c, r) }
        report("meadowGrass") { c, r -> MeadowDecoration.GRASS.decorate(c, r) }
        report("meadowFlowersAt") { c, r ->
            MeadowDecoration.placeAt(c, r, DecorationPos(c.chunkX * 16 + 8, 71, c.chunkZ * 16 + 8))
        }
        report("flowerForest") { c, r -> FlowerForestDecoration.decorate(c, r) }
        report("flowerForestFlowers") { c, r -> FlowerForestDecoration.FLOWERS_ONLY.decorate(c, r) }
        report("birchWildflowers") { c, r -> ForestFloorDecoration.BIRCH_WILDFLOWERS.decorate(c, r) }
        report("meadowWildflowers") { c, r -> ForestFloorDecoration.MEADOW_WILDFLOWERS.decorate(c, r) }
        report("leafLitter") { c, r -> ForestFloorDecoration.LEAF_LITTER.decorate(c, r) }
        // Podzol carries no grass block, so the leaf litter filter rejects every candidate.
        report("leafLitterWrongGround", surface = "podzol") { c, r ->
            ForestFloorDecoration.LEAF_LITTER.decorate(c, r)
        }

        // The two noise state providers on their own, sampled over a fixed lattice.
        val flowerForest = StringBuilder()
        val meadow = StringBuilder()
        for (i in 0 until 40) {
            val x = i * 37 - 400
            val z = i * -53 + 250
            flowerForest.append(FlowerForestDecoration.flowerAtForTest(x, 71, z)).append(',')
            meadow.append(MeadowDecoration.flowerStateAtForTest(DecorationPos(x, 71, z))).append(',')
        }
        println("ORACLE noise flowerForest=$flowerForest meadow=$meadow")
    }
}
