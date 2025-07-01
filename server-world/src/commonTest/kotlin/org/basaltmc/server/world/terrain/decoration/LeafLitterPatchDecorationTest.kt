package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class LeafLitterPatchDecorationTest {
    @Test
    fun leafLitterMatchesOfficialVegetalDecorationVector() {
        val chunk = flatBank(Blocks.GRASS_BLOCK.stateId)
        val trace = Trace()

        LeafLitterPatchDecoration.decorate(
            chunk,
            1234L,
            environment("dark_forest"),
            externalBlockState = { _, y, _ -> bankState(y, Blocks.GRASS_BLOCK.stateId) },
            blockWriteObserver = trace::write,
        )

        assertEquals(15, trace.writes)
        assertEquals(58908183927633548L, trace.hash)
        val vector = vector(chunk)
        assertContentEquals(intArrayOf(26624, 256, 71412, 12), vector.counts)
        assertEquals(-2366248879061323763L, vector.categoryHash)
        assertEquals(8535262442401353564L, vector.rawStateHash)
    }

    @Test
    fun leafLitterRejectsOtherBiomes() {
        val chunk = flatBank(Blocks.GRASS_BLOCK.stateId)
        val trace = Trace()

        LeafLitterPatchDecoration.decorate(
            chunk,
            1234L,
            environment("forest"),
            externalBlockState = { _, y, _ -> bankState(y, Blocks.GRASS_BLOCK.stateId) },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
    }

    @Test
    fun leafLitterRequiresGrassBlockBelow() {
        val chunk = flatBank(Blocks.DIRT.stateId)
        val trace = Trace()

        LeafLitterPatchDecoration.decorate(
            chunk,
            1234L,
            environment("dark_forest"),
            externalBlockState = { _, y, _ -> bankState(y, Blocks.DIRT.stateId) },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
    }

    private fun flatBank(surfaceStateId: Int): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, bankState(y, surfaceStateId))
                    }
                }
            }
        }

    private fun bankState(
        y: Int,
        surfaceStateId: Int,
    ): Int =
        when {
            y < 40 -> Blocks.DIRT.stateId
            y == 40 -> surfaceStateId
            else -> Blocks.AIR.stateId
        }

    private fun vector(chunk: Chunk): Vector {
        val paths = listOf("dirt", "grass_block", "air", "leaf_litter")
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
}
