package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the thirteen-spec flower dispatcher. */
class FlowerPatchOracle {
    private inner class FakeEnvironment(private val biome: String) : SurfaceRuleEnvironment {
        override fun surfaceDepthNoise(x: Int, z: Int): Int = 0
        override fun surfaceSecondaryNoise(x: Int, z: Int): Double = 0.0
        override fun sample2d(noiseId: String, x: Int, z: Int): Double = 0.0
        override fun sample3d(noiseId: String, x: Int, y: Int, z: Int): Double = 0.0
        override fun verticalGradientRandom(name: String, x: Int, y: Int, z: Int): Float = 0.5f
        override fun positionalRandom(x: Int, y: Int, z: Int): BasaltRandom = XoroshiroRandom(x * 31L + z)
        override fun estimatedSurfaceHeight(x: Int, z: Int): Int = 70
        override fun biomeAt(x: Int, y: Int, z: Int): String = biome
        override fun noiseBiomeAt(quartX: Int, quartY: Int, quartZ: Int, cache: DensityCache?): String = biome
        override fun isSteep(x: Int, z: Int): Boolean = false
        override fun clayBand(x: Int, y: Int, z: Int): Int = 0
    }

    /** Grass with a leaf canopy over half of it, so the no-leaves heightmap spec differs. */
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

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var plants = 0
        for (y in 69..80) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (!Blocks.isAir(id) && id != Blocks.STONE.stateId &&
                        id != Blocks.GRASS_BLOCK.stateId && id != Blocks.OAK_LEAVES.stateId
                    ) {
                        plants++
                    }
                }
            }
        }
        return checksum to plants
    }

    private fun report(name: String, biome: String, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var plants = 0
        val distinct = mutableSetOf<Int>()
        for (index in 0 until 80) {
            val chunk = groundChunk(index * 3 - 40, index * -5 + 15)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkPlants) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            plants += chunkPlants
            for (y in 69..80) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        val id = chunk.getBlockStateId(x, y, z)
                        if (!Blocks.isAir(id) && id != Blocks.STONE.stateId &&
                            id != Blocks.GRASS_BLOCK.stateId && id != Blocks.OAK_LEAVES.stateId
                        ) {
                            distinct.add(id)
                        }
                    }
                }
            }
        }
        println("ORACLE $name plants=$plants distinct=${distinct.size} checksum=$checksum")
    }

    @Test
    fun dump() {
        for (biome in listOf(
            "minecraft:plains", "minecraft:flower_forest", "minecraft:pale_garden", "minecraft:meadow",
            "minecraft:swamp", "minecraft:cherry_grove", "minecraft:birch_forest", "minecraft:savanna",
            "minecraft:taiga",
        )) {
            report(biome.substringAfter(':'), biome) { c, e -> FlowerPatchDecoration.decorate(c, 31415L, e) }
        }
        // the_void is in none of the thirteen biome lists, so this is the real rejection case.
        report("wrong", "minecraft:the_void") { c, e -> FlowerPatchDecoration.decorate(c, 31415L, e) }
        for (name in listOf("flower_default", "flower_plains", "wildflowers_meadow", "pale_garden_flowers")) {
            val biome = when (name) {
                "flower_plains" -> "minecraft:plains"
                "wildflowers_meadow" -> "minecraft:meadow"
                "pale_garden_flowers" -> "minecraft:pale_garden"
                else -> "minecraft:taiga"
            }
            report("only_$name", biome) { c, e -> FlowerPatchDecoration.decorateFeature(c, 31415L, e, name, null) }
        }
    }
}
