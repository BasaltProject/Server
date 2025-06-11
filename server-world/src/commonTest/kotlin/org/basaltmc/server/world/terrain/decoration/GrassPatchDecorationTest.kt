package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class GrassPatchDecorationTest {
    @Test
    fun grassPatchesMatchOfficialVegetalDecorationVectors() {
        for (case in CASES) {
            val chunk = flatChunk()
            val trace = Trace()

            GrassPatchDecoration.decorate(
                chunk,
                1234L,
                environment(case.biome),
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
    fun grassPatchesRejectOtherBiomes() {
        val chunk = flatChunk()

        GrassPatchDecoration.decorate(
            chunk,
            1234L,
            environment("the_void"),
            externalBlockState = { _, y, _ -> flatState(y) },
        )

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
        val paths = listOf("dirt", "grass_block", "air", "short_grass", "fern")
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
                    "patch_grass_jungle",
                    "jungle",
                    intArrayOf(26624, 256, 71322, 76, 26),
                    141481314855002497L,
                    -1675785001297243317L,
                    147,
                    -6520743694944141465L,
                ),
                Case(
                    "patch_grass_savanna",
                    "savanna",
                    intArrayOf(26624, 256, 71336, 88, 0),
                    -1246555086614302887L,
                    -1587331823885956159L,
                    137,
                    6558223643049505468L,
                ),
                Case(
                    "patch_grass_meadow",
                    "meadow",
                    intArrayOf(26624, 256, 71392, 32, 0),
                    2221433969994377505L,
                    -8520675746712768511L,
                    43,
                    -699696558662885482L,
                ),
                Case(
                    "patch_grass_plain",
                    "plains",
                    intArrayOf(26624, 256, 71374, 50, 0),
                    -3742699765279453261L,
                    -5110371055440305647L,
                    73,
                    -3158108069729065243L,
                ),
                Case(
                    "patch_grass_taiga",
                    "old_growth_pine_taiga",
                    intArrayOf(26624, 256, 71385, 6, 33),
                    6020611903157758793L,
                    1156392050108085466L,
                    52,
                    4597504363327658158L,
                ),
                Case(
                    "patch_grass_forest",
                    "forest",
                    intArrayOf(26624, 256, 71413, 11, 0),
                    1951190023761675980L,
                    -7286512445410593383L,
                    15,
                    -762772397494473235L,
                ),
                Case(
                    "patch_grass_taiga_2",
                    "taiga",
                    intArrayOf(26624, 256, 71421, 0, 3),
                    -1297034353751656313L,
                    -7757385366692115684L,
                    6,
                    7713552300338260160L,
                ),
                Case(
                    "patch_grass_normal",
                    "swamp",
                    intArrayOf(26624, 256, 71406, 18, 0),
                    8022079784049194899L,
                    7929798209573292305L,
                    35,
                    2627497103749035716L,
                ),
                Case(
                    "patch_grass_badlands",
                    "beach",
                    intArrayOf(26624, 256, 71421, 3, 0),
                    5504296871420058820L,
                    -7342502825601818791L,
                    8,
                    1150780669848359417L,
                ),
            )
    }
}
