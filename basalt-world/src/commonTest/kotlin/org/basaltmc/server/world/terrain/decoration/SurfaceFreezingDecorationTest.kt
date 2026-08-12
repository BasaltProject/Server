package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertEquals

class SurfaceFreezingDecorationTest {
    @Test
    fun snowyBiomeAddsSnowToEveryExposedSolidColumn() {
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)

        SurfaceFreezingDecoration.decorate(chunk, environment("minecraft:snowy_plains"))

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                assertEquals(Blocks.SNOW.stateId, chunk.getBlockStateId(x, 63, z))
                assertEquals(Blocks.GRASS_BLOCK.stateId - 1, chunk.getBlockStateId(x, 62, z))
            }
        }
    }

    @Test
    fun snowMarksEverySnowyBlockTypeBelowIt() {
        val chunk = flatChunk(Blocks.DIRT.stateId)
        val surfaceStates =
            listOf(
                Blocks.GRASS_BLOCK.stateId,
                Blocks.PODZOL.stateId,
                stateId("mycelium"),
            )
        surfaceStates.forEachIndexed { x, stateId -> chunk.setBlockStateId(x, 62, 0, stateId) }

        SurfaceFreezingDecoration.decorate(chunk, environment("minecraft:snowy_plains"))

        surfaceStates.forEachIndexed { x, stateId ->
            assertEquals(Blocks.SNOW.stateId, chunk.getBlockStateId(x, 63, 0))
            assertEquals(stateId - 1, chunk.getBlockStateId(x, 62, 0))
        }
        assertEquals(Blocks.DIRT.stateId, chunk.getBlockStateId(3, 62, 0))
    }

    @Test
    fun frozenRiverTurnsEveryExposedWaterColumnToIceWithoutSnowOnTop() {
        val chunk = flatChunk(Blocks.WATER.stateId)

        SurfaceFreezingDecoration.decorate(chunk, environment("minecraft:frozen_river"))

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                assertEquals(Blocks.ICE.stateId, chunk.getBlockStateId(x, 62, z))
                assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(x, 63, z))
            }
        }
    }

    @Test
    fun warmBiomeLeavesTheSameSurfaceUntouched() {
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)

        SurfaceFreezingDecoration.decorate(chunk, environment("minecraft:plains"))

        for (x in 0 until 16) {
            for (z in 0 until 16) assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(x, 63, z))
        }
    }

    @Test
    fun evaluatesTheBiomeOfEachColumnInsteadOfTheChunkCenterOnly() {
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
        val splitEnvironment = environment { x -> if (x < 8) "minecraft:snowy_plains" else "minecraft:plains" }

        SurfaceFreezingDecoration.decorate(chunk, splitEnvironment)

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val expected = if (x < 8) Blocks.SNOW.stateId else Blocks.AIR.stateId
                assertEquals(expected, chunk.getBlockStateId(x, 63, z))
            }
        }
    }

    private fun flatChunk(surfaceStateId: Int): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, surfaceStateId)
            }
        }

    private fun environment(biomeId: String): SurfaceRuleEnvironment = environment { biomeId }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla(path),
            )?.stateId
            ?: error("unknown block '$path'")

    private fun environment(biomeForX: (Int) -> String): SurfaceRuleEnvironment =
        object : SurfaceRuleEnvironment {
            override fun surfaceDepthNoise(
                x: Int,
                z: Int,
            ): Int = 0

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
            ): Float = 0.0f

            override fun estimatedSurfaceHeight(
                x: Int,
                z: Int,
            ): Int = 63

            override fun biomeAt(
                x: Int,
                y: Int,
                z: Int,
            ): String = biomeForX(x)

            override fun isSteep(
                x: Int,
                z: Int,
            ): Boolean = false
        }
}
