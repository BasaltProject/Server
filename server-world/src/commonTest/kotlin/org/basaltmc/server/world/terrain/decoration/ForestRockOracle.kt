package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the forest rock. */
class ForestRockOracle {
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
     * Supporting ground under a gap of air: the rock walks down until it finds something it may sit
     * on, so a solid column would never exercise that search and a hollow one would never terminate
     * it.
     */
    private fun terrainChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60 until 68) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                // A stripe of non-supporting ground, so the downward walk really has to move.
                chunk.setBlockStateId(x, 68, z, if (x % 3 == 0) stateId("sand") else Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var rock = 0
        for (y in 58..76) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("mossy_cobblestone")) rock++
                }
            }
        }
        return checksum to rock
    }

    private fun report(name: String, biome: String) {
        var checksum = -3750763034362895579L
        var rock = 0
        for (index in 0 until 80) {
            val chunk = terrainChunk(index * 3 - 40, index * -5 + 15)
            ForestRockDecoration.decorate(chunk, 606L, FakeEnvironment(biome))
            val (chunkChecksum, chunkRock) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            rock += chunkRock
        }
        println("ORACLE $name rock=$rock checksum=$checksum")
    }

    @Test
    fun dump() {
        report("pineTaiga", "minecraft:old_growth_pine_taiga")
        report("spruceTaiga", "minecraft:old_growth_spruce_taiga")
        // plains is in neither old-growth list, so this is the real rejection case.
        report("wrong", "minecraft:plains")

        // The blob itself at a fixed origin, so the three-step walk and the radii are pinned.
        val sizes = StringBuilder()
        val chunk = terrainChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(191919L))
        for (i in 0 until 12) {
            val before = fold(chunk).second
            ForestRockDecoration.placeAt(chunk, random, 2 + i % 12, 72, 3 + (i * 5) % 12)
            sizes.append(fold(chunk).second - before).append(',')
        }
        println("ORACLE sizes=$sizes checksum=" + fold(chunk).first + " after=" + random.nextInt())
    }
}
