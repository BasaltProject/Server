package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the four sugar-cane patches. */
class SugarCaneOracle {
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

    /** A shoreline: sand banks beside a water channel, which is the only place cane accepts. */
    private fun shoreChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 66 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, if (z in 6..9) Blocks.WATER.stateId else stateId("sand"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var cane = 0
        for (y in 69..78) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == Blocks.SUGAR_CANE.stateId) cane++
                }
            }
        }
        return checksum to cane
    }

    private fun report(name: String, biome: String, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var cane = 0
        var deferred = 0
        for (index in 0 until 120) {
            val chunk = shoreChunk(index * 3 - 60, index * -5 + 20)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkCane) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            cane += chunkCane
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name cane=$cane deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("desert", "minecraft:desert") { c, e -> SugarCanePatchDecoration.decorate(c, 13579L, e) }
        report("badlands", "minecraft:badlands") { c, e -> SugarCanePatchDecoration.decorate(c, 13579L, e) }
        report("swamp", "minecraft:swamp") { c, e -> SugarCanePatchDecoration.decorate(c, 13579L, e) }
        report("plains", "minecraft:plains") { c, e -> SugarCanePatchDecoration.decorate(c, 13579L, e) }
        // deep_dark is in none of the four lists, so this is the real rejection case.
        report("wrong", "minecraft:deep_dark") { c, e -> SugarCanePatchDecoration.decorate(c, 13579L, e) }
        report("desertOnly", "minecraft:desert") { c, e ->
            SugarCanePatchDecoration.decorateFeature(c, 13579L, e, "patch_sugar_cane_desert")
        }
        report("regional", "minecraft:desert") { c, e ->
            SugarCanePatchDecoration.decorate(c, 13579L, e, externalBlockState = { _, y, z ->
                if (y in 66..69) Blocks.STONE.stateId
                else if (y == 70) (if ((z and 15) in 6..9) Blocks.WATER.stateId else stateId("sand"))
                else Blocks.AIR.stateId
            })
        }
    }
}
