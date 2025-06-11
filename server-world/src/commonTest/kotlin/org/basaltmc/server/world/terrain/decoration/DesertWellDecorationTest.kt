package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class DesertWellDecorationTest {
    @Test
    fun desertWellMatchesOfficialSurfaceStructureVector() {
        val chunk = sandChunk()

        DesertWellDecoration.decorate(chunk, 1274L, environment("desert"))

        val (hash, counts) = vector(chunk)
        assertContentEquals(intArrayOf(32441, 89, 12, 5, 2), counts)
        assertEquals(6765957456212819724L, hash)
        assertEquals(2, chunk.blockEntities().size)
        for (blockEntity in chunk.blockEntities()) {
            assertEquals(Identifier.vanilla("brushable_block"), blockEntity.type)
            assertEquals("minecraft:archaeology/desert_well", blockEntity.data.getString("LootTable"))
        }
    }

    @Test
    fun rarityAndBiomeFiltersRejectPlacement() {
        val commonSeed = sandChunk()
        DesertWellDecoration.decorate(commonSeed, 1234L, environment("desert"))
        assertContentEquals(intArrayOf(32512, 0, 0, 0, 0), vector(commonSeed).second)

        val wrongBiome = sandChunk()
        DesertWellDecoration.decorate(wrongBiome, 1274L, environment("badlands"))
        assertContentEquals(intArrayOf(32512, 0, 0, 0, 0), vector(wrongBiome).second)
    }

    private fun sandChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..62) chunk.setBlockStateId(x, y, z, sandStateId)
                }
            }
        }

    private fun vector(chunk: Chunk): Pair<Long, IntArray> {
        val paths = listOf("sand", "sandstone", "sandstone_slab", "water", "suspicious_sand")
        val categories = paths.map(::stateId).withIndex().associate { it.value to it.index }
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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing minecraft:$path")

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

    private val sandStateId = stateId("sand")
}
