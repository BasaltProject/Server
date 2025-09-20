package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isDoublePlantState
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the sunflower and large-fern patches. */
class DoublePlantOracle {
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

    /**
     * A mix of supporting and non-supporting ground, plus a low ceiling over one stripe: a double
     * plant needs two free blocks, so the ceiling is what exercises the upper-half rejection.
     */
    private fun groundChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, if (x % 5 == 0) Blocks.STONE.stateId else Blocks.GRASS_BLOCK.stateId)
                if (z % 6 == 0) chunk.setBlockStateId(x, 72, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var lower = 0
        var halves = 0
        for (y in 69..78) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.isDoublePlantState(id)) {
                        halves++
                        if (id == stateId("sunflower") || id == stateId("large_fern")) lower++
                    }
                }
            }
        }
        return Triple(checksum, lower, halves)
    }

    private fun report(name: String, biome: String, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var lower = 0
        var halves = 0
        var deferred = 0
        for (index in 0 until 150) {
            val chunk = groundChunk(index * 3 - 70, index * -5 + 25)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkLower, chunkHalves) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            lower += chunkLower
            halves += chunkHalves
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name lower=$lower halves=$halves deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("sunflower", "minecraft:sunflower_plains") { c, e ->
            DoublePlantPatchDecoration.decorate(c, 8080L, e)
        }
        report("largeFernTaiga", "minecraft:taiga") { c, e -> DoublePlantPatchDecoration.decorate(c, 8080L, e) }
        report("largeFernSnowy", "minecraft:snowy_taiga") { c, e ->
            DoublePlantPatchDecoration.decorate(c, 8080L, e)
        }
        // desert is in neither spec's list, so this is the real rejection case.
        report("wrong", "minecraft:desert") { c, e -> DoublePlantPatchDecoration.decorate(c, 8080L, e) }
        report("sunflowerOnly", "minecraft:sunflower_plains") { c, e ->
            DoublePlantPatchDecoration.decorateFeature(c, 8080L, e, "patch_sunflower")
        }
        report("regional", "minecraft:sunflower_plains") { c, e ->
            DoublePlantPatchDecoration.decorate(c, 8080L, e, externalBlockState = { _, y, _ ->
                if (y in 66..69) Blocks.STONE.stateId
                else if (y == 70) Blocks.GRASS_BLOCK.stateId
                else Blocks.AIR.stateId
            })
        }

        val chunk = groundChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(5566L))
        val ok = DoublePlantPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "patch_large_fern")
        val bad = DoublePlantPatchDecoration.placeAt(chunk, random, DecorationPos(8, 71, 8), "patch_nothing")
        val (checksum, lower, halves) = fold(chunk)
        println("ORACLE direct=$ok,$bad lower=$lower halves=$halves checksum=$checksum after=" + random.nextInt())
        println("ORACLE ids=" + DoublePlantPatchDecoration.PLACED_IDS.sorted().joinToString(","))
    }
}
