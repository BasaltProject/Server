package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isGlowLichenState
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the glow lichen growth. */
class GlowLichenOracle {
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
     * Stone with open shafts through it, and a water column in one: lichen attaches to the shaft
     * walls, so a solid mass gives it nowhere to sit and open air gives it nothing to attach to. The
     * water exercises the waterlogged state, which is a different bit than the faces.
     */
    private fun shaftChunk(chunkX: Int, chunkZ: Int): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in -60 until 100) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        for (x in 2 until 15 step 4) {
            for (z in 2 until 15 step 4) {
                for (y in -55 until 95) {
                    chunk.setBlockStateId(x, y, z, if (x == 2 && z == 2) Blocks.WATER.stateId else Blocks.AIR.stateId)
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var lichen = 0
        val distinct = HashSet<Int>()
        for (y in -60..100) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (Blocks.isGlowLichenState(id)) {
                        lichen++
                        distinct.add(id)
                    }
                }
            }
        }
        return Triple(checksum, lichen, distinct.size)
    }

    @Test
    fun dump() {
        for (biome in listOf("minecraft:lush_caves", "minecraft:the_void")) {
            var checksum = -3750763034362895579L
            var lichen = 0
            var distinct = 0
            for (index in 0 until 25) {
                val chunk = shaftChunk(index * 3 - 12, index * -5 + 8)
                GlowLichenDecoration.decorate(chunk, 515151L, FakeEnvironment(biome))
                val (chunkChecksum, chunkLichen, chunkDistinct) = fold(chunk)
                checksum = (checksum xor chunkChecksum) * 1099511628211L
                lichen += chunkLichen
                distinct += chunkDistinct
            }
            println("ORACLE ${biome.substringAfter(':')} lichen=$lichen distinct=$distinct checksum=$checksum")
        }

        // One growth at a shaft wall, so the face bits and the spread step are pinned directly.
        val states = StringBuilder()
        var placed = 0
        for (index in 0 until 24) {
            val chunk = shaftChunk(0, 0)
            val random = WorldgenRandom(LegacyRandom(6161L + index * 6364136223846793005L))
            if (GlowLichenDecoration.placeAt(chunk, random, DecorationPos(2, -50 + index, 6))) placed++
            states.append(fold(chunk).second).append(',')
        }
        println("ORACLE direct=$placed states=$states")
    }
}
