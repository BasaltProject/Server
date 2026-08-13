package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class TallGrassDecorationTest {
    @Test
    fun savannaTallGrassMatchesOfficialVegetalDecorationVector() {
        val chunk = flatChunk(0)
        val trace = Trace()

        TallGrassDecoration.decorate(
            chunk,
            2L,
            environment("savanna"),
            externalBlockState = { _, y, _ -> flatState(y) },
            blockWriteObserver = trace::write,
        )

        assertEquals(44, trace.writes)
        assertEquals(-2146010454616710797L, trace.hash)
        val vector = vector(chunk)
        assertContentEquals(intArrayOf(26624, 256, 71410, 14), vector.counts)
        assertEquals(8170729439661878305L, vector.categoryHash)
        assertEquals(-4569111753175441368L, vector.rawStateHash)
    }

    @Test
    fun plainTallGrassMatchesOfficialVegetalDecorationVector() {
        val chunk = flatChunk(10)
        val trace = Trace()

        TallGrassDecoration.decorate(
            chunk,
            2L,
            environment("plains"),
            externalBlockState = { _, y, _ -> flatState(y) },
            blockWriteObserver = trace::write,
        )

        assertEquals(54, trace.writes)
        assertEquals(943831762055608331L, trace.hash)
        val vector = vector(chunk)
        assertContentEquals(intArrayOf(26624, 256, 71384, 40), vector.counts)
        assertEquals(-1675226741621084543L, vector.categoryHash)
        assertEquals(3194262218729897109L, vector.rawStateHash)
    }

    @Test
    fun tallGrassFeaturesRejectOtherBiomes() {
        val chunk = flatChunk(0)

        TallGrassDecoration.decorate(
            chunk,
            2L,
            environment("the_void"),
            externalBlockState = { _, y, _ -> flatState(y) },
        )

        assertContentEquals(intArrayOf(26624, 256, 71424, 0), vector(chunk).counts)
    }

    private fun flatChunk(chunkX: Int): Chunk =
        Chunk(chunkX, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, flatState(y))
                    }
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
        val paths = listOf("dirt", "grass_block", "air", "tall_grass")
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
