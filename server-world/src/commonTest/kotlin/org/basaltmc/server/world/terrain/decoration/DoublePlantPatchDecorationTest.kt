package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class DoublePlantPatchDecorationTest {
    @Test
    fun doublePlantPatchesMatchOfficialVegetalDecorationVectors() {
        for (case in CASES) {
            val chunk = flatChunk()
            val trace = Trace()

            DoublePlantPatchDecoration.decorateFeature(
                chunk,
                case.seed,
                environment(case.biome),
                case.name,
                externalBlockState = { _, y, _ -> flatState(y) },
                blockWriteObserver = trace::write,
            )

            assertEquals(case.writes, trace.writes, "${case.name} writes")
            assertEquals(case.traceHash, trace.hash, "${case.name} trace")
            val vector = vector(chunk)
            assertContentEquals(case.counts, vector.counts, "${case.name} counts")
            assertEquals(case.categoryHash, vector.categoryHash, "${case.name} category hash")
            assertEquals(case.rawStateHash, vector.rawStateHash, "${case.name} raw-state hash")
        }
    }

    @Test
    fun doublePlantPatchesRejectOtherBiomes() {
        val chunk = flatChunk()
        val trace = Trace()

        DoublePlantPatchDecoration.decorate(
            chunk,
            2L,
            environment("the_void"),
            externalBlockState = { _, y, _ -> flatState(y) },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
        assertContentEquals(intArrayOf(26624, 256, 71424, 0, 0), vector(chunk).counts)
    }

    private fun flatChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) chunk.setBlockStateId(x, y, z, flatState(y))
                }
            }
        }

    private fun flatState(y: Int): Int =
        when {
            y < 40 -> Blocks.DIRT.stateId
            y == 40 -> Blocks.GRASS_BLOCK.stateId
            else -> Blocks.AIR.stateId
        }

    private fun vector(chunk: Chunk): Vector {
        val paths = listOf("dirt", "grass_block", "air", "sunflower", "large_fern")
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

    private class Trace {
        var hash: Long = 1L
        var writes: Int = 0

        fun write(
            x: Int,
            y: Int,
            z: Int,
            stateId: Int,
        ) {
            hash = 31L * hash + x
            hash = 31L * hash + y
            hash = 31L * hash + z
            hash = 31L * hash + stateId
            writes++
        }
    }

    private data class Vector(
        val categoryHash: Long,
        val rawStateHash: Long,
        val counts: IntArray,
    )

    private data class Case(
        val name: String,
        val biome: String,
        val seed: Long,
        val counts: IntArray,
        val categoryHash: Long,
        val rawStateHash: Long,
        val writes: Int,
        val traceHash: Long,
    )

    private companion object {
        val CASES =
            listOf(
                Case(
                    "patch_sunflower",
                    "sunflower_plains",
                    2L,
                    intArrayOf(26624, 256, 71404, 20, 0),
                    7899473884845533121L,
                    8900036784792845259L,
                    34,
                    -181056490292152895L,
                ),
                Case(
                    "patch_large_fern",
                    "taiga",
                    14L,
                    intArrayOf(26624, 256, 71388, 0, 36),
                    2753545991560359041L,
                    3864674802480823379L,
                    50,
                    4587153096977197129L,
                ),
            )
    }
}
