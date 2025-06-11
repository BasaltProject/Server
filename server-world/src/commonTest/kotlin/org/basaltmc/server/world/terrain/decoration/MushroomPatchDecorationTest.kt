package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class MushroomPatchDecorationTest {
    @Test
    fun mushroomPatchesMatchOfficialVegetalDecorationVectors() {
        for (case in CASES) {
            val chunk = flatChunk()
            val trace = Trace()

            MushroomPatchDecoration.decorateFeature(
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
    fun mushroomPatchesRejectOtherBiomes() {
        val chunk = flatChunk()
        val trace = Trace()

        MushroomPatchDecoration.decorate(
            chunk,
            0L,
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
        val paths = listOf("dirt", "grass_block", "air", "brown_mushroom", "red_mushroom")
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
                    "brown_mushroom_old_growth",
                    "old_growth_pine_taiga",
                    16L,
                    intArrayOf(26624, 256, 71406, 18, 0),
                    2788163188409649043L,
                    -2079726853880362175L,
                    18,
                    -793344804184235983L,
                ),
                Case(
                    "red_mushroom_old_growth",
                    "old_growth_pine_taiga",
                    305L,
                    intArrayOf(26624, 256, 71415, 0, 9),
                    -6256063429660244077L,
                    6466402988405671018L,
                    29,
                    -5515695936809544361L,
                ),
                Case(
                    "brown_mushroom_swamp",
                    "swamp",
                    0L,
                    intArrayOf(26624, 256, 71397, 27, 0),
                    -3417672738748028196L,
                    228389241493177697L,
                    37,
                    -4767524857668485309L,
                ),
                Case(
                    "red_mushroom_swamp",
                    "swamp",
                    85L,
                    intArrayOf(26624, 256, 71411, 0, 13),
                    3554487072837398939L,
                    5443223346415205870L,
                    20,
                    -2146626125952045633L,
                ),
                Case(
                    "brown_mushroom_normal",
                    "plains",
                    462L,
                    intArrayOf(26624, 256, 71405, 19, 0),
                    -3006184548216938284L,
                    2234110489222528097L,
                    23,
                    -7524101901060154935L,
                ),
                Case(
                    "red_mushroom_normal",
                    "plains",
                    461L,
                    intArrayOf(26624, 256, 71405, 0, 19),
                    1740333745460322471L,
                    6980628782899788852L,
                    23,
                    2953589282318046048L,
                ),
                Case(
                    "brown_mushroom_taiga",
                    "taiga",
                    3L,
                    intArrayOf(26624, 256, 71413, 11, 0),
                    -7985463998564677940L,
                    -7913919086079790239L,
                    23,
                    7516749172351986786L,
                ),
                Case(
                    "red_mushroom_taiga",
                    "taiga",
                    454L,
                    intArrayOf(26624, 256, 71405, 0, 19),
                    1740333745460322471L,
                    6980628782899788852L,
                    23,
                    2953589282318046048L,
                ),
            )
    }
}
