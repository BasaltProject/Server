package org.basaltmc.world.terrain

import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.noise.NormalNoise
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class RealTerrainGeneratorTest {
    private fun syntheticNoise(name: String): NormalNoise {
        val seed = name.hashCode().toLong()
        return NormalNoise.create(XoroshiroRandom(seed), -5, doubleArrayOf(1.0, 1.0, 1.0, 1.0, 1.0))
    }

    private val base3d = BlendedNoise.createUnseeded(0.25, 0.125, 80.0, 160.0, 8.0)
    private val router = NoiseRouterData.overworld(::syntheticNoise, base3d)

    @Test
    fun everyColumnHasBedrockAtTheBottom() {
        val chunk = RealTerrainGenerator.generate(router, 0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(x, WorldHeight.MIN_Y, z))
            }
        }
    }

    @Test
    fun producesBothSolidAndAirBlocksNotADegenerateChunk() {
        val chunk = RealTerrainGenerator.generate(router, 0, 0)
        var solid = 0
        var air = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    if (chunk.getBlockStateId(x, y, z) == Blocks.AIR.stateId) air++ else solid++
                }
            }
        }
        assertTrue(solid > 0, "expected at least some solid blocks")
        assertTrue(air > 0, "expected at least some air blocks")
    }

    @Test
    fun differentChunksProduceDifferentTerrain() {
        val a = RealTerrainGenerator.generate(router, 0, 0)
        val b = RealTerrainGenerator.generate(router, 5, 5)
        var anyDifference = false
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    if (a.getBlockStateId(x, y, z) != b.getBlockStateId(x, y, z)) anyDifference = true
                }
            }
        }
        assertTrue(anyDifference, "expected chunk (0,0) and (5,5) to differ")
    }

    @Test
    fun isDeterministicForTheSameChunkPosition() {
        val a = RealTerrainGenerator.generate(router, 3, -2)
        val b = RealTerrainGenerator.generate(router, 3, -2)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    assertEquals(a.getBlockStateId(x, y, z), b.getBlockStateId(x, y, z))
                }
            }
        }
    }
}
