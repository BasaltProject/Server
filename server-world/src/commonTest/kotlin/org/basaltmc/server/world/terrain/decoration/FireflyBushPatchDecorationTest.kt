package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class FireflyBushPatchDecorationTest {
    @Test
    fun fireflyBushPatchesMatchOfficialVegetalDecorationVectors() {
        for (case in CASES) {
            val chunk = checkerboardBank(Blocks.WATER.stateId)
            val trace = Trace()

            FireflyBushPatchDecoration.decorateFeature(
                chunk,
                case.seed,
                environment(case.biome),
                case.name,
                externalBlockState = { x, y, z -> bankState(x, y, z, Blocks.WATER.stateId) },
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
    fun fireflyBushPatchesRejectOtherBiomes() {
        val chunk = checkerboardBank(Blocks.WATER.stateId)
        val trace = Trace()

        FireflyBushPatchDecoration.decorate(
            chunk,
            1234L,
            environment("the_void"),
            externalBlockState = { x, y, z -> bankState(x, y, z, Blocks.WATER.stateId) },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
    }

    @Test
    fun nearWaterFirefliesRequireActualWaterFluid() {
        val frostedIce = stateId("frosted_ice")
        val chunk = checkerboardBank(frostedIce)
        val trace = Trace()

        FireflyBushPatchDecoration.decorateFeature(
            chunk,
            1234L,
            environment("plains"),
            "patch_firefly_bush_near_water",
            externalBlockState = { x, y, z -> bankState(x, y, z, frostedIce) },
            blockWriteObserver = trace::write,
        )

        assertEquals(0, trace.writes)
    }

    private fun checkerboardBank(adjacentStateId: Int): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, bankState(x, y, z, adjacentStateId))
                    }
                }
            }
        }

    private fun bankState(
        x: Int,
        y: Int,
        z: Int,
        adjacentStateId: Int,
    ): Int =
        when {
            y < 40 -> Blocks.DIRT.stateId
            y == 40 && (x + z) and 1 == 0 -> Blocks.GRASS_BLOCK.stateId
            y == 40 -> adjacentStateId
            else -> Blocks.AIR.stateId
        }

    private fun vector(chunk: Chunk): Vector {
        val paths = listOf("dirt", "grass_block", "water", "air", "firefly_bush")
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

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

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
                    "patch_firefly_bush_swamp",
                    "swamp",
                    8L,
                    intArrayOf(26624, 128, 128, 71421, 3),
                    463297027280149828L,
                    -758215198038180728L,
                    4,
                    -8694575369527323081L,
                ),
                Case(
                    "patch_firefly_bush_near_water_swamp",
                    "swamp",
                    1234L,
                    intArrayOf(26624, 128, 128, 71424, 0),
                    -8130986790494260863L,
                    3699909999488622721L,
                    1,
                    1054807L,
                ),
                Case(
                    "patch_firefly_bush_near_water",
                    "plains",
                    1234L,
                    intArrayOf(26624, 128, 128, 71422, 2),
                    -6436491792130251773L,
                    3860400912145866203L,
                    2,
                    919225835425L,
                ),
            )
    }
}
