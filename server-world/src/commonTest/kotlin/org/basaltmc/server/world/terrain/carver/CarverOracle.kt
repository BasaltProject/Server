package org.basaltmc.server.world.terrain.carver

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.noise.NormalNoise
import org.basaltmc.server.world.terrain.NoiseRouterData
import org.basaltmc.server.world.terrain.OverworldAquifer
import org.basaltmc.server.world.terrain.surface.SurfaceRuleData
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.math.abs
import kotlin.math.sin
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the cave and canyon carvers. */
class CarverOracle {
    private fun syntheticNoise(id: String): NormalNoise {
        var hash = -3750763034362895579L
        for (character in id) hash = (hash xor character.code.toLong()) * 1099511628211L
        val source = XoroshiroRandom(hash)
        return NormalNoise.create(source, -(id.length % 5 + 1), doubleArrayOf(1.0, 0.6, 0.3))
    }

    private fun fake(name: String, x: Int, y: Int, z: Int): Double {
        var hash = -3750763034362895579L
        for (character in name) hash = (hash xor character.code.toLong()) * 1099511628211L
        hash = (hash xor x.toLong()) * 1099511628211L
        hash = (hash xor y.toLong()) * 1099511628211L
        hash = (hash xor z.toLong()) * 1099511628211L
        return sin(hash.toDouble() * 1.0E-17)
    }

    private inner class FakeEnvironment : SurfaceRuleEnvironment {
        override fun surfaceDepthNoise(x: Int, z: Int): Int = ((x * 31 + z * 17) and 7) - 2
        override fun surfaceSecondaryNoise(x: Int, z: Int): Double = fake("secondary", x, 0, z)
        override fun sample2d(noiseId: String, x: Int, z: Int): Double = fake(noiseId, x, 0, z)
        override fun sample3d(noiseId: String, x: Int, y: Int, z: Int): Double = fake(noiseId, x, y, z)
        override fun verticalGradientRandom(name: String, x: Int, y: Int, z: Int): Float =
            abs(fake(name, x, y, z)).toFloat()
        override fun positionalRandom(x: Int, y: Int, z: Int): BasaltRandom = XoroshiroRandom(x * 31L + z)
        override fun estimatedSurfaceHeight(x: Int, z: Int): Int = 64 + ((x * 7 + z * 13) and 31)
        override fun biomeAt(x: Int, y: Int, z: Int): String = "minecraft:plains"
        override fun noiseBiomeAt(quartX: Int, quartY: Int, quartZ: Int, cache: DensityCache?): String =
            "minecraft:plains"
        override fun isSteep(x: Int, z: Int): Boolean = ((x xor z) and 3) == 0
        override fun clayBand(x: Int, y: Int, z: Int): Int = 1000
    }

    @Test
    fun dump() {
        val router = NoiseRouterData.overworld(::syntheticNoise, BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0))
        val environment = FakeEnvironment()
        val chunk = Chunk(3, -5)
        // Solid stone up to y=100 so every carve has something to cut, plus a grass and dirt cap
        // so the top-material repair path runs too.
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y until 98) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                chunk.setBlockStateId(x, 98, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, 99, z, Blocks.GRASS_BLOCK.stateId)
            }
        }
        val aquifer = OverworldAquifer(router, 987L, 3, -5, WorldHeight.MIN_Y, WorldHeight.HEIGHT)
        OverworldCarvers.carve(chunk, 987L, aquifer, environment, SurfaceRuleData.overworld())

        var checksum = -3750763034362895579L
        var carved = 0
        for (y in WorldHeight.MIN_Y until WorldHeight.MIN_Y + WorldHeight.HEIGHT) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor stateId.toLong()) * 1099511628211L
                    if (Blocks.isAir(stateId) && y < 98) carved++
                }
            }
        }
        println("ORACLE carve.checksum=$checksum carved=$carved")
    }

    @Test
    fun dumpNether() {
        val chunk = Chunk(2, 7, org.basaltmc.server.world.ChunkHeight.NETHER)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 0 until 128) {
                    chunk.setBlockStateId(x, y, z, Blocks.NETHERRACK.stateId)
                }
            }
        }
        NetherCaveCarver.carve(chunk, 4242L)

        var checksum = -3750763034362895579L
        var carved = 0
        for (y in 0 until 128) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor stateId.toLong()) * 1099511628211L
                    if (stateId != Blocks.NETHERRACK.stateId) carved++
                }
            }
        }
        println("ORACLE nether.checksum=$checksum carved=$carved")
    }
}
