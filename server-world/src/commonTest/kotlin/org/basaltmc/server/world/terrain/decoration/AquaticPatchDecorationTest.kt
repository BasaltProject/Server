package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class AquaticPatchDecorationTest {
    @Test
    fun aquaticFeaturesMatchOfficialVegetalDecorationVectors() {
        for (case in CASES) {
            val chunk = oceanChunk(case.chunkX)
            val trace = Trace()
            AquaticPatchDecoration.decorateFeature(
                chunk,
                case.seed,
                environment(case.biome),
                case.name,
                externalBlockState = { _, y, _ -> oceanState(y) },
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
    fun aquaticFeaturesRejectBiomesWithoutTheirPlacedFeature() {
        val chunk = oceanChunk()
        val trace = Trace()
        AquaticPatchDecoration.decorateFeature(
            chunk,
            1234L,
            environment("plains"),
            "seagrass_swamp",
            externalBlockState = { _, y, _ -> oceanState(y) },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
    }

    private fun oceanChunk(chunkX: Int = 0): Chunk =
        Chunk(chunkX, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..55) chunk.setBlockStateId(x, y, z, oceanState(y))
                }
            }
        }

    private fun oceanState(y: Int): Int =
        when {
            y < 40 -> Blocks.STONE.stateId
            y <= 55 -> Blocks.WATER.stateId
            else -> Blocks.AIR.stateId
        }

    private fun vector(chunk: Chunk): Vector {
        val paths = listOf("air", "stone", "water", "seagrass", "tall_seagrass", "sea_pickle", "kelp_plant", "kelp")
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
        val chunkX: Int,
        val seed: Long,
        val writes: Int,
        val traceHash: Long,
        val categoryHash: Long,
        val rawStateHash: Long,
        val counts: IntArray,
    )

    private companion object {
        val CASES =
            listOf(
                Case(
                    "seagrass_swamp",
                    "swamp",
                    0,
                    1234L,
                    73,
                    -1532755421349321790L,
                    7488501095956436149L,
                    5656555384622197848L,
                    intArrayOf(67584, 26624, 4066, 12, 18, 0, 0, 0),
                ),
                Case(
                    "seagrass_river",
                    "river",
                    0,
                    1234L,
                    61,
                    -7543284328742160225L,
                    -2155003019103241991L,
                    8047964064134038585L,
                    intArrayOf(67584, 26624, 4072, 8, 16, 0, 0, 0),
                ),
                Case(
                    "seagrass_warm",
                    "warm_ocean",
                    0,
                    1234L,
                    79,
                    1730745208162860381L,
                    8161705660080708895L,
                    9213709634519987715L,
                    intArrayOf(67584, 26624, 4034, 34, 28, 0, 0, 0),
                ),
                Case(
                    "sea_pickle",
                    "warm_ocean",
                    0,
                    35L,
                    17,
                    8204753542463122919L,
                    4561128427494032617L,
                    7943623421856389637L,
                    intArrayOf(67584, 26624, 4088, 0, 0, 8, 0, 0),
                ),
                Case(
                    "seagrass_deep_warm",
                    "deep_lukewarm_ocean",
                    0,
                    1234L,
                    105,
                    8868908665996883247L,
                    -4578442912763714309L,
                    1481252041009138946L,
                    intArrayOf(67584, 26624, 3996, 6, 94, 0, 0, 0),
                ),
                Case(
                    "kelp_warm",
                    "lukewarm_ocean",
                    -38,
                    1234L,
                    39,
                    438654355660970446L,
                    -4532838828998734314L,
                    582073408137770322L,
                    intArrayOf(67584, 26624, 4057, 0, 0, 0, 32, 7),
                ),
                Case(
                    "seagrass_normal",
                    "ocean",
                    0,
                    1234L,
                    49,
                    -7681197328855607443L,
                    3768839734218869677L,
                    -57847932245774184L,
                    intArrayOf(67584, 26624, 4058, 20, 18, 0, 0, 0),
                ),
                Case(
                    "seagrass_deep",
                    "deep_ocean",
                    0,
                    1234L,
                    65,
                    -2442676531961504670L,
                    -6756274948893476233L,
                    -6301760449433460934L,
                    intArrayOf(67584, 26624, 4040, 10, 46, 0, 0, 0),
                ),
                Case(
                    "seagrass_cold",
                    "cold_ocean",
                    0,
                    1234L,
                    38,
                    2116102886868892800L,
                    -2759697607858318572L,
                    4494708616127185122L,
                    intArrayOf(67584, 26624, 4069, 13, 14, 0, 0, 0),
                ),
                Case(
                    "seagrass_deep_cold",
                    "deep_cold_ocean",
                    0,
                    1234L,
                    63,
                    -4231425536006384936L,
                    8690932756895940283L,
                    -2974853257803532517L,
                    intArrayOf(67584, 26624, 4046, 6, 44, 0, 0, 0),
                ),
                Case(
                    "kelp_cold",
                    "ocean",
                    -38,
                    1234L,
                    66,
                    748089552387291553L,
                    7011521698132276647L,
                    69294459397623149L,
                    intArrayOf(67584, 26624, 4030, 0, 0, 0, 56, 10),
                ),
            )
    }
}
