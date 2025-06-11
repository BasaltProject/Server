package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class GroundPlantPatchDecorationTest {
    @Test
    fun groundPlantPatchesMatchOfficialVegetalDecorationVectors() {
        for (case in CASES) {
            val chunk = flatChunk(case.chunkX, case.surface)
            val trace = Trace()

            GroundPlantPatchDecoration.decorateFeature(
                chunk,
                1234L,
                environment(case.biome),
                case.name,
                externalBlockState = { _, y, _ -> flatState(y, case.surface) },
                blockWriteObserver = trace::write,
            )

            assertEquals(case.writes, trace.writes, "${case.name} writes")
            assertEquals(case.traceHash, trace.hash, "${case.name} trace")
            val vector = vector(chunk, case.paths)
            assertContentEquals(case.counts, vector.counts, "${case.name} counts")
            assertEquals(case.categoryHash, vector.categoryHash, "${case.name} category hash")
            assertEquals(case.rawStateHash, vector.rawStateHash, "${case.name} raw-state hash")
        }
    }

    @Test
    fun overlappingGroundPlantPatchesMatchOfficialCombinedVectors() {
        for (case in COMBINED_CASES) {
            val chunk = flatChunk(case.chunkX, "sand")
            val trace = Trace()

            GroundPlantPatchDecoration.decorate(
                chunk,
                1234L,
                environment(case.biome),
                externalBlockState = { _, y, _ -> flatState(y, "sand") },
                blockWriteObserver = trace::write,
            )

            assertEquals(case.writes, trace.writes, "${case.biome} combined writes")
            assertEquals(case.traceHash, trace.hash, "${case.biome} combined trace")
            val vector = vector(chunk, COMBINED_PATHS)
            assertContentEquals(case.counts, vector.counts, "${case.biome} combined counts")
            assertEquals(case.categoryHash, vector.categoryHash, "${case.biome} combined category hash")
            assertEquals(case.rawStateHash, vector.rawStateHash, "${case.biome} combined raw-state hash")
        }
    }

    @Test
    fun groundPlantPatchesRejectOtherBiomes() {
        val chunk = flatChunk(0, "grass_block")
        val trace = Trace()

        GroundPlantPatchDecoration.decorate(
            chunk,
            1234L,
            environment("the_void"),
            externalBlockState = { _, y, _ -> flatState(y, "grass_block") },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
        assertContentEquals(
            intArrayOf(26624, 256, 71424, 0, 0, 0),
            vector(chunk, DEAD_BUSH_PATHS).counts,
        )
    }

    private fun flatChunk(
        chunkX: Int,
        surface: String,
    ): Chunk =
        Chunk(chunkX, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, flatState(y, surface))
                    }
                }
            }
        }

    private fun flatState(
        y: Int,
        surface: String,
    ): Int =
        when {
            y < 40 -> Blocks.DIRT.stateId
            y == 40 -> state(surface)
            else -> Blocks.AIR.stateId
        }

    private fun vector(
        chunk: Chunk,
        paths: List<String>,
    ): Vector {
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
        val surface: String,
        val paths: List<String>,
        val counts: IntArray,
        val categoryHash: Long,
        val rawStateHash: Long,
        val writes: Int,
        val traceHash: Long,
    )

    private data class CombinedCase(
        val biome: String,
        val chunkX: Int,
        val counts: IntArray,
        val categoryHash: Long,
        val rawStateHash: Long,
        val writes: Int,
        val traceHash: Long,
    )

    private companion object {
        val DEAD_BUSH_PATHS = listOf("dirt", "grass_block", "air", "dead_bush", "short_dry_grass", "tall_dry_grass")
        val DRY_GRASS_PATHS = listOf("dirt", "sand", "air", "short_dry_grass", "tall_dry_grass")
        val COMBINED_PATHS = listOf("dirt", "sand", "air", "dead_bush", "short_dry_grass", "tall_dry_grass")

        val CASES =
            listOf(
                Case(
                    "patch_dead_bush",
                    "swamp",
                    0,
                    "grass_block",
                    DEAD_BUSH_PATHS,
                    intArrayOf(26624, 256, 71422, 2, 0, 0),
                    -7059632148959632509L,
                    -7703622403955269227L,
                    2,
                    974209474994L,
                ),
                Case(
                    "patch_dry_grass_desert",
                    "desert",
                    10,
                    "sand",
                    DRY_GRASS_PATHS,
                    intArrayOf(26624, 256, 71416, 7, 1),
                    -5944873673054546870L,
                    3520434043248021922L,
                    14,
                    5659435297559779655L,
                ),
                Case(
                    "patch_dead_bush_2",
                    "desert",
                    0,
                    "grass_block",
                    DEAD_BUSH_PATHS,
                    intArrayOf(26624, 256, 71423, 1, 0, 0),
                    -6028201936876021182L,
                    7171345570476264523L,
                    2,
                    1194453801818L,
                ),
                Case(
                    "patch_dry_grass_badlands",
                    "badlands",
                    0,
                    "sand",
                    DRY_GRASS_PATHS,
                    intArrayOf(26624, 256, 71414, 5, 5),
                    -1270224619496018992L,
                    -6853564835465595330L,
                    15,
                    6088862932117845788L,
                ),
                Case(
                    "patch_dead_bush_badlands",
                    "badlands",
                    0,
                    "grass_block",
                    DEAD_BUSH_PATHS,
                    intArrayOf(26624, 256, 71412, 12, 0, 0),
                    -2150543263953653747L,
                    6593412781185078905L,
                    17,
                    8452212923577145552L,
                ),
            )

        val COMBINED_CASES =
            listOf(
                CombinedCase(
                    "desert",
                    10,
                    intArrayOf(26624, 256, 71415, 1, 7, 1),
                    -1093769140396783725L,
                    3801019708303468268L,
                    15,
                    -886686790535247736L,
                ),
                CombinedCase(
                    "badlands",
                    0,
                    intArrayOf(26624, 256, 71403, 11, 5, 5),
                    8646653282718764581L,
                    -7961671486745329556L,
                    31,
                    8833350444904111593L,
                ),
            )

        fun state(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
    }
}
