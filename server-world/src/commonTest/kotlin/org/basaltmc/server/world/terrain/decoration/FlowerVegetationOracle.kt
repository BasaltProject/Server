package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the flower, mushroom and jungle grass patches. */
class FlowerVegetationOracle {
    private fun groundChunk(
        chunkX: Int = -7,
        chunkZ: Int = 4,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 71) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                // Podzol in a stripe so the jungle patch's not-on-podzol filter really rejects.
                chunk.setBlockStateId(x, 70, z, if (z % 3 == 0) stateId("podzol") else Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }

    /**
     * Over several chunks rather than one: these patches sit behind rarity filters of up to 1/512,
     * so a single chunk mostly measures the filter rejecting and never reaches the placement itself.
     */
    private fun report(
        name: String,
        body: (Chunk, WorldgenRandom) -> Unit,
    ) {
        var checksum = -3750763034362895579L
        var plants = 0
        var after = 0
        for (index in 0 until 40) {
            val chunk = groundChunk(-7 + index * 3, 4 - index * 5)
            // Sequential legacy seeds leave the first draw correlated, and a rarity filter reads
            // exactly that draw: spaced seeds are what let these patches ever pass their filter.
            val random = WorldgenRandom(LegacyRandom(24680L + index * 6364136223846793005L))
            body(chunk, random)
            for (y in 68..80) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        val id = chunk.getBlockStateId(x, y, z)
                        checksum = (checksum xor id.toLong()) * 1099511628211L
                        if (!Blocks.isAir(id) &&
                            id != Blocks.STONE.stateId &&
                            id != Blocks.GRASS_BLOCK.stateId &&
                            id != stateId("podzol")
                        ) {
                            plants++
                        }
                    }
                }
            }
            after = after * 31 + random.nextInt()
        }
        println("ORACLE $name plants=$plants checksum=$checksum after=$after")
    }

    private fun chunkOriginX(): Int = -7 * 16 + 4

    private fun chunkOriginZ(): Int = 4 * 16 + 4

    @Test
    fun dump() {
        report("flowerNormal") { c, r -> ForestFlowerDecoration.NORMAL.decorate(c, r) }
        report("flowerForest") { c, r -> ForestFlowerDecoration.FLOWER_FOREST.decorate(c, r) }
        report(
            "flowerAt",
        ) { c, r -> ForestFlowerDecoration.placeAt(c, r, DecorationPos(chunkOriginX(), 71, chunkOriginZ())) }
        report("mushroomNormal") { c, r -> MushroomDecoration.NORMAL.decorate(c, r) }
        report("mushroomTaiga") { c, r -> MushroomDecoration.TAIGA.decorate(c, r) }
        report("mushroomOldGrowth") { c, r -> MushroomDecoration.OLD_GROWTH.decorate(c, r) }
        report("mushroomSwamp") { c, r -> MushroomDecoration.SWAMP.decorate(c, r) }
        report("jungleGrass") { c, r -> JungleGrassDecoration.decorate(c, r) }
        report(
            "jungleAt",
        ) { c, r -> JungleGrassDecoration.placePatchAt(c, r, DecorationPos(chunkOriginX(), 71, chunkOriginZ())) }
    }
}
