package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class IceSpikesDecorationTest {
    @Test
    fun spikesAndPatchesMatchOfficialSurfaceStructureVector() {
        val chunk = snowChunk()

        IceSpikesDecoration.decorate(chunk, 1234L, environment("ice_spikes"))

        val (hash, counts) = vector(chunk)
        assertContentEquals(intArrayOf(32068, 208, 354), counts)
        assertEquals(-7801114596328259961L, hash)
    }

    @Test
    fun biomeFilterRejectsOtherBiomes() {
        val chunk = snowChunk()

        IceSpikesDecoration.decorate(chunk, 1234L, environment("snowy_plains"))

        assertContentEquals(intArrayOf(32256, 256, 0), vector(chunk).second)
    }

    private fun snowChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y until 62) chunk.setBlockStateId(x, y, z, Blocks.DIRT.stateId)
                    chunk.setBlockStateId(x, 62, z, Blocks.SNOW_BLOCK.stateId)
                }
            }
        }

    private fun vector(chunk: Chunk): Pair<Long, IntArray> {
        val categories =
            mapOf(
                Blocks.DIRT.stateId to 0,
                Blocks.SNOW_BLOCK.stateId to 1,
                Blocks.PACKED_ICE.stateId to 2,
            )
        val counts = IntArray(categories.size)
        var hash = 1L
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val category = categories[chunk.getBlockStateId(x, y, z)] ?: -1
                    hash = 31L * hash + category
                    if (category >= 0) counts[category]++
                }
            }
        }
        return hash to counts
    }

    private fun environment(biome: String): SurfaceRuleEnvironment =
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
            ): String = "minecraft:$biome"

            override fun isSteep(
                x: Int,
                z: Int,
            ): Boolean = false
        }
}
