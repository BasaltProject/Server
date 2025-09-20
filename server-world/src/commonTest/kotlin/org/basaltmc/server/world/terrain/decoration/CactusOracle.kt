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

/** The frozen Kotlin as the oracle for the cactus column and its two patches. */
class CactusOracle {
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

    /** Sand flats: cacti need sand below, air around and air above. */
    private fun sandChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -64 until 70) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 70, z, stateId("sand"))
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Pair<Long, Int> {
        var checksum = -3750763034362895579L
        var cacti = 0
        for (y in 69..80) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id == stateId("cactus") || id == stateId("cactus_flower")) cacti++
                }
            }
        }
        return checksum to cacti
    }

    private fun report(name: String, biome: String, body: (Chunk, FakeEnvironment) -> Unit) {
        var checksum = -3750763034362895579L
        var cacti = 0
        var deferred = 0
        for (index in 0 until 24) {
            val chunk = sandChunk(index * 5 - 30, index * -3 + 12)
            body(chunk, FakeEnvironment(biome))
            val (chunkChecksum, chunkCacti) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            cacti += chunkCacti
            deferred += chunk.takeGeneratedBlockWrites().size
        }
        println("ORACLE $name cacti=$cacti deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("desert", "minecraft:desert") { c, e -> CactusPatchDecoration.decorate(c, 5150L, e) }
        report("badlands", "minecraft:badlands") { c, e -> CactusPatchDecoration.decorate(c, 5150L, e) }
        report("wrongBiome", "minecraft:plains") { c, e -> CactusPatchDecoration.decorate(c, 5150L, e) }
        report("desertOnly", "minecraft:desert") { c, e ->
            CactusPatchDecoration.decorateFeature(c, 5150L, e, "patch_cactus_desert")
        }
        report("regional", "minecraft:desert") { c, e ->
            CactusPatchDecoration.decorate(c, 5150L, e, externalBlockState = { _, y, _ ->
                if (y < 70) Blocks.STONE.stateId else if (y == 70) stateId("sand") else Blocks.AIR.stateId
            })
        }

        // The column on its own, so the height and flower truncation is pinned directly.
        val heights = StringBuilder()
        val chunk = sandChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(31415L))
        for (i in 0 until 40) {
            val x = i % 16
            val z = (i * 7) % 16
            val before = fold(chunk).second
            CactusFeature.place(chunk, random, DecorationPos(x, 71, z))
            heights.append(fold(chunk).second - before).append(',')
        }
        println("ORACLE column=$heights checksum=" + fold(chunk).first + " after=" + random.nextInt())
    }
}
