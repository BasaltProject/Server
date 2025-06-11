package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class ForestRockDecorationTest {
    @Test
    fun forestRockMatchesOfficialLocalModificationVector() {
        val chunk = dirtChunk()

        ForestRockDecoration.decorate(chunk, 1234L, environment("old_growth_pine_taiga"))

        val (hash, counts) = vector(chunk)
        assertContentEquals(intArrayOf(32504, 14), counts)
        assertEquals(298145945176655689L, hash)
    }

    @Test
    fun forestRockRejectsNormalTaiga() {
        val chunk = dirtChunk()
        ForestRockDecoration.decorate(chunk, 1234L, environment("taiga"))
        assertContentEquals(intArrayOf(32512, 0), vector(chunk).second)
    }

    private fun dirtChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..62) chunk.setBlockStateId(x, y, z, Blocks.DIRT.stateId)
                }
            }
        }

    private fun vector(chunk: Chunk): Pair<Long, IntArray> {
        val mossy =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("mossy_cobblestone"))?.stateId
                ?: error("missing mossy cobblestone")
        val categories = mapOf(Blocks.DIRT.stateId to 0, mossy to 1)
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
