package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class BambooDecorationTest {
    @Test
    fun denseBambooMatchesOfficialVegetalDecorationVector() {
        val chunk = flatChunk()
        var traceHash = 1L
        var writes = 0

        BambooDecoration.decorate(
            chunk,
            1234L,
            environment("bamboo_jungle"),
            externalBlockState = { _, y, _ -> flatState(y) },
            blockWriteObserver = { x, y, z, stateId ->
                traceHash = 31L * traceHash + x
                traceHash = 31L * traceHash + y
                traceHash = 31L * traceHash + z
                traceHash = 31L * traceHash + stateId
                writes++
            },
        )

        assertEquals(637, writes)
        assertEquals(-6031861822473765261L, traceHash)
        val vector = vector(chunk)
        assertContentEquals(intArrayOf(26624, 185, 71, 70948, 476), vector.counts)
        assertEquals(-7585828635518356498L, vector.categoryHash)
        assertEquals(-8411888671712128701L, vector.rawStateHash)
    }

    @Test
    fun lightBambooMatchesOfficialVegetalDecorationVector() {
        val chunk = flatChunk()

        BambooDecoration.decorate(
            chunk,
            1234L,
            environment("jungle"),
            externalBlockState = { _, y, _ -> flatState(y) },
        )

        val vector = vector(chunk)
        assertContentEquals(intArrayOf(26624, 256, 0, 71410, 14), vector.counts)
        assertEquals(-181811451422513887L, vector.categoryHash)
        assertEquals(-7089172836712523778L, vector.rawStateHash)
    }

    @Test
    fun bambooFeaturesRejectOtherBiomes() {
        val chunk = flatChunk()

        BambooDecoration.decorate(
            chunk,
            1234L,
            environment("plains"),
            externalBlockState = { _, y, _ -> flatState(y) },
        )

        assertContentEquals(intArrayOf(26624, 256, 0, 71424, 0), vector(chunk).counts)
    }

    @Test
    fun denseBambooPlacesStalksAndPodzolOnSupportedGround() {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y until 40) chunk.setBlockStateId(x, y, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, 40, z, Blocks.GRASS_BLOCK.stateId)
            }
        }

        BambooDecoration.DENSE.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        val states = (0 until chunk.sectionCount).flatMap { chunk.section(it).blockStateIds().asList() }
        assertTrue(states.count { Blocks.byStateId(it)?.block == Blocks.BAMBOO.block } > 20)
        assertTrue(states.any { it == Blocks.PODZOL.stateId })
    }

    private fun flatChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
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
        val paths = listOf("dirt", "grass_block", "podzol", "air", "bamboo")
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
