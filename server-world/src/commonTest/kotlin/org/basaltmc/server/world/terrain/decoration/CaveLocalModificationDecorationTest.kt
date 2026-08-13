package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class CaveLocalModificationDecorationTest {
    @Test
    fun amethystGeodeMatchesOfficialLocalModificationVector() {
        val chunk = solidStoneChunk()

        CaveLocalModificationDecoration.decorateFeatures(
            chunk,
            12L,
            environment("plains"),
            listOf(2),
            externalBlockState = { _, _, _ -> Blocks.STONE.stateId },
        )

        val vector = vector(chunk)
        assertContentEquals(intArrayOf(97431, 308, 157, 11, 183, 210, 1, 0, 1, 2, 0), vector.counts)
        assertEquals(242806153789775310L, vector.categoryHash)
        assertEquals(735844853917495735L, vector.rawStateHash)
    }

    @Test
    fun largeDripstonesMatchOfficialLocalModificationVector() {
        val chunk = cavePatternChunk()

        CaveLocalModificationDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("dripstone_caves"),
            listOf(3),
            externalSurfaceHeight = { _, _ -> WorldHeight.MAX_Y + 1 },
            externalBlockState = { x, y, z -> cavePatternState(x, y, z) },
        )

        val vector = vector(chunk)
        assertContentEquals(intArrayOf(32768, 61223, 0, 0, 0, 0, 0, 0, 0, 0, 4313), vector.counts)
        assertEquals(8579572668889789306L, vector.categoryHash)
        assertEquals(120177866851292673L, vector.rawStateHash)
    }

    @Test
    fun biomeFiltersRejectMissingLocalModifications() {
        val geode = solidStoneChunk()
        CaveLocalModificationDecoration.decorateFeatures(
            geode,
            12L,
            environment("the_void"),
            listOf(2),
            externalBlockState = { _, _, _ -> Blocks.STONE.stateId },
        )
        assertContentEquals(intArrayOf(98304, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), vector(geode).counts)

        val dripstone = cavePatternChunk()
        CaveLocalModificationDecoration.decorateFeatures(
            dripstone,
            1234L,
            environment("plains"),
            listOf(3),
            externalSurfaceHeight = { _, _ -> WorldHeight.MAX_Y + 1 },
            externalBlockState = { x, y, z -> cavePatternState(x, y, z) },
        )
        assertContentEquals(intArrayOf(32768, 65536, 0, 0, 0, 0, 0, 0, 0, 0, 0), vector(dripstone).counts)
    }

    private fun solidStoneChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                    }
                }
            }
        }

    private fun cavePatternChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, cavePatternState(x, y, z))
                    }
                }
            }
        }

    private fun cavePatternState(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val layer = (y - WorldHeight.MIN_Y).mod(24)
        return if (layer in 4..19) Blocks.AIR.stateId else Blocks.STONE.stateId
    }

    private fun vector(chunk: Chunk): Vector {
        val paths =
            listOf(
                "stone",
                "air",
                "amethyst_block",
                "budding_amethyst",
                "calcite",
                "smooth_basalt",
                "small_amethyst_bud",
                "medium_amethyst_bud",
                "large_amethyst_bud",
                "amethyst_cluster",
                "dripstone_block",
            )
        val categories = paths.withIndex().associate { it.value to it.index }
        val counts = IntArray(paths.size)
        var categoryHash = 1L
        var rawStateHash = 1L
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    val category =
                        Blocks
                            .byStateId(stateId)
                            ?.block
                            ?.path
                            ?.let(categories::get) ?: -1
                    categoryHash = 31L * categoryHash + category
                    rawStateHash = 31L * rawStateHash + stateId
                    if (category >= 0) counts[category]++
                }
            }
        }
        return Vector(categoryHash, rawStateHash, counts)
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
            ): Int = WorldHeight.MAX_Y + 1

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

    private data class Vector(
        val categoryHash: Long,
        val rawStateHash: Long,
        val counts: IntArray,
    )
}
