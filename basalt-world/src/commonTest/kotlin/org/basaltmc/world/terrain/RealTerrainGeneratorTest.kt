package org.basaltmc.world.terrain

import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.WorldHeight
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.noise.NormalNoise
import org.basaltmc.world.terrain.surface.SurfaceRuleEnvironment
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

    // A minimal but real environment: `estimatedSurfaceHeight` scans the same density function the
    // chunk itself was filled from (matching what a real implementation does), everything else is a
    // reasonable constant stand-in (no real Noises.SURFACE/SURFACE_SECONDARY/biome data needed to
    // exercise RealTerrainGenerator's own wiring, which is what this test class covers).
    private val environment =
        object : SurfaceRuleEnvironment {
            override fun surfaceDepthNoise(
                x: Int,
                z: Int,
            ): Int = 3

            override fun surfaceSecondaryNoise(
                x: Int,
                z: Int,
            ): Double = 0.0

            override fun sample2d(
                noiseId: String,
                x: Int,
                z: Int,
            ): Double = 0.0

            override fun sample3d(
                noiseId: String,
                x: Int,
                y: Int,
                z: Int,
            ): Double = 0.0

            override fun verticalGradientRandom(
                name: String,
                x: Int,
                y: Int,
                z: Int,
            ): Float = 1.0f

            override fun estimatedSurfaceHeight(
                x: Int,
                z: Int,
            ): Int {
                var y = WorldHeight.MAX_Y
                while (y >= WorldHeight.MIN_Y && router.finalDensity.compute(FunctionContext(x, y, z)) <= 0.0) y--
                return y + 1
            }

            override fun biomeAt(
                x: Int,
                y: Int,
                z: Int,
            ): String = "minecraft:plains"

            override fun isSteep(
                x: Int,
                z: Int,
            ): Boolean = false
        }

    @Test
    fun everyColumnHasBedrockAtTheBottom() {
        val chunk = RealTerrainGenerator.generate(router, environment, 0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(x, WorldHeight.MIN_Y, z))
            }
        }
    }

    @Test
    fun producesBothSolidAndAirBlocksNotADegenerateChunk() {
        val chunk = RealTerrainGenerator.generate(router, environment, 0, 0)
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
        val a = RealTerrainGenerator.generate(router, environment, 0, 0)
        val b = RealTerrainGenerator.generate(router, environment, 5, 5)
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
        val a = RealTerrainGenerator.generate(router, environment, 3, -2)
        val b = RealTerrainGenerator.generate(router, environment, 3, -2)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    assertEquals(a.getBlockStateId(x, y, z), b.getBlockStateId(x, y, z))
                }
            }
        }
    }

    @Test
    fun theTopmostSolidBlockOfEachColumnIsGrass() {
        val chunk = RealTerrainGenerator.generate(router, environment, 0, 0)
        val heightmap = chunk.heightmap()
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val topY = heightmap[x + z * 16] - 1
                assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(x, topY, z), "at ($x, $z), top=$topY")
            }
        }
    }
}
