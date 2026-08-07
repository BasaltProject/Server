package org.basaltmc.world.terrain.surface

import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class FakeEnvironment(
    private val heights: IntArray,
) : SurfaceRuleEnvironment {
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
    ): Int = heights[(x and 15) + (z and 15) * 16]

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

private fun flatChunk(surfaceY: Int): Chunk {
    val chunk = Chunk(0, 0)
    for (x in 0 until 16) {
        for (z in 0 until 16) {
            for (y in WorldHeight.MIN_Y..surfaceY) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
        }
    }
    return chunk
}

class SurfaceBuilderTest {
    @Test
    fun grassOnTopDirtBelowForAPlainsColumn() {
        val surfaceY = 70
        val chunk = flatChunk(surfaceY)
        val environment = FakeEnvironment(IntArray(256) { surfaceY + 1 })
        SurfaceBuilder.buildSurface(chunk, environment, SurfaceRuleData.overworld())

        assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(5, surfaceY, 5))
        // Real surfaceDepth here is fixed at 3 (FakeEnvironment): ON_FLOOR/UNDER_FLOOR's stone-depth
        // check treats the 3 blocks under the grass as still "under the floor", so they read DIRT.
        assertEquals(Blocks.DIRT.stateId, chunk.getBlockStateId(5, surfaceY - 1, 5))
        assertEquals(Blocks.DIRT.stateId, chunk.getBlockStateId(5, surfaceY - 2, 5))
    }

    @Test
    fun deepStoneStaysStoneFarBelowTheSurface() {
        val surfaceY = 70
        val chunk = flatChunk(surfaceY)
        val environment = FakeEnvironment(IntArray(256) { surfaceY + 1 })
        SurfaceBuilder.buildSurface(chunk, environment, SurfaceRuleData.overworld())

        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(5, 20, 5))
    }

    @Test
    fun deepslateBlendNearTheWorldFloor() {
        val surfaceY = 70
        val chunk = flatChunk(surfaceY)
        val environment = FakeEnvironment(IntArray(256) { surfaceY + 1 })
        SurfaceBuilder.buildSurface(chunk, environment, SurfaceRuleData.overworld())

        val deepslateStateId =
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("deepslate"),
                )!!
                .stateId
        assertEquals(deepslateStateId, chunk.getBlockStateId(5, 0, 5))
        assertEquals(deepslateStateId, chunk.getBlockStateId(5, -10, 5))
    }

    @Test
    fun bedrockAtTheWorldFloor() {
        val surfaceY = 70
        val chunk = flatChunk(surfaceY)
        val environment = FakeEnvironment(IntArray(256) { surfaceY + 1 })
        SurfaceBuilder.buildSurface(chunk, environment, SurfaceRuleData.overworld())

        assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(5, WorldHeight.MIN_Y, 5))
    }

    @Test
    fun everyColumnGetsAGrassSurface() {
        val surfaceY = 65
        val chunk = flatChunk(surfaceY)
        val environment = FakeEnvironment(IntArray(256) { surfaceY + 1 })
        SurfaceBuilder.buildSurface(chunk, environment, SurfaceRuleData.overworld())

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(x, surfaceY, z), "at ($x, $z)")
            }
        }
    }

    @Test
    fun isDeterministic() {
        val surfaceY = 68
        val environment = FakeEnvironment(IntArray(256) { surfaceY + 1 })
        val chunkA = flatChunk(surfaceY)
        val chunkB = flatChunk(surfaceY)
        SurfaceBuilder.buildSurface(chunkA, environment, SurfaceRuleData.overworld())
        SurfaceBuilder.buildSurface(chunkB, environment, SurfaceRuleData.overworld())

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..surfaceY) {
                    assertEquals(chunkA.getBlockStateId(x, y, z), chunkB.getBlockStateId(x, y, z))
                }
            }
        }
    }

    @Test
    fun airAboveTheSurfaceIsUntouched() {
        val surfaceY = 70
        val chunk = flatChunk(surfaceY)
        val environment = FakeEnvironment(IntArray(256) { surfaceY + 1 })
        SurfaceBuilder.buildSurface(chunk, environment, SurfaceRuleData.overworld())

        assertTrue(chunk.getBlockStateId(5, surfaceY + 5, 5) == Blocks.AIR.stateId)
    }
}
