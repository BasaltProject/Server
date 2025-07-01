package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class VinePatchDecorationTest {
    @Test
    fun vinesMatchOfficialVegetalDecorationVector() {
        val chunk = vineLattice()
        val trace = Trace()

        VinePatchDecoration.decorate(
            chunk,
            1234L,
            environment("jungle"),
            externalBlockState = { x, y, _ -> latticeState(x, y) },
            blockWriteObserver = trace::write,
        )

        assertEquals(62, trace.writes)
        assertEquals(-7981831261894827739L, trace.hash)
        val vector = vector(chunk)
        assertContentEquals(intArrayOf(93506, 4736, 62), vector.counts)
        assertEquals(-3514734851662086259L, vector.categoryHash)
        assertEquals(-1178608608212683751L, vector.rawStateHash)
    }

    @Test
    fun vinesRejectOtherBiomes() {
        val chunk = vineLattice()
        val trace = Trace()

        VinePatchDecoration.decorate(
            chunk,
            1234L,
            environment("forest"),
            externalBlockState = { x, y, _ -> latticeState(x, y) },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
    }

    private fun vineLattice(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, latticeState(x, y))
                    }
                }
            }
        }

    private fun latticeState(
        x: Int,
        y: Int,
    ): Int =
        if (x and 1 == 0 && y in 64..100) {
            Blocks.STONE.stateId
        } else {
            Blocks.AIR.stateId
        }

    private fun vector(chunk: Chunk): Vector {
        val paths = listOf("air", "stone", "vine")
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
