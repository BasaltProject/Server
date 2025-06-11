package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class OverworldSpringDecorationTest {
    @Test
    fun normalSpringsMatchOfficialFluidSpringsVector() {
        val chunk = patternedChunk(stoneStateId)

        OverworldSpringDecoration.decorate(chunk, 1234L, environment("plains"), ::normalState)

        val (hash, counts) = vector(chunk)
        assertContentEquals(intArrayOf(94204, 0, 4093, 2, 5), counts)
        assertEquals(-7256632957596669683L, hash)
        assertEquals(7, chunk.scheduledFluidTicks().size)
    }

    @Test
    fun frozenSpringsMatchOfficialFluidSpringsVector() {
        val chunk = patternedChunk(Blocks.PACKED_ICE.stateId)

        OverworldSpringDecoration.decorate(chunk, 1234L, environment("grove"), ::frozenState)

        val (hash, counts) = vector(chunk)
        assertContentEquals(intArrayOf(0, 94206, 4094, 2, 2), counts)
        assertEquals(-6267808401443110071L, hash)
        assertEquals(4, chunk.scheduledFluidTicks().size)
    }

    @Test
    fun voidBiomeAndNonFrozenBiomeRejectTheirMissingFeatures() {
        val voidChunk = patternedChunk(stoneStateId)
        OverworldSpringDecoration.decorate(voidChunk, 1234L, environment("the_void"), ::normalState)
        assertContentEquals(intArrayOf(94208, 0, 4096, 0, 0), vector(voidChunk).second)
        assertEquals(0, voidChunk.scheduledFluidTicks().size)

        val snowyTaiga = patternedChunk(Blocks.PACKED_ICE.stateId)
        OverworldSpringDecoration.decorateFeatures(
            snowyTaiga,
            1234L,
            environment("snowy_taiga"),
            listOf(2),
            ::frozenState,
        )
        assertContentEquals(intArrayOf(0, 94208, 4096, 0, 0), vector(snowyTaiga).second)
        assertEquals(0, snowyTaiga.scheduledFluidTicks().size)
    }

    private fun patternedChunk(solidStateId: Int): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        if (!isPatternAir(x, y, z)) chunk.setBlockStateId(x, y, z, solidStateId)
                    }
                }
            }
        }

    private fun normalState(
        x: Int,
        y: Int,
        z: Int,
    ): Int = if (isPatternAir(x, y, z)) Blocks.AIR.stateId else stoneStateId

    private fun frozenState(
        x: Int,
        y: Int,
        z: Int,
    ): Int = if (isPatternAir(x, y, z)) Blocks.AIR.stateId else Blocks.PACKED_ICE.stateId

    private fun isPatternAir(
        x: Int,
        y: Int,
        z: Int,
    ): Boolean =
        y.mod(3) == 0 &&
            (x.mod(4) == 0 || x.mod(4) == 1) &&
            z.mod(4) == 0

    private fun vector(chunk: Chunk): Pair<Long, IntArray> {
        val categories =
            listOf(
                stoneStateId,
                Blocks.PACKED_ICE.stateId,
                Blocks.AIR.stateId,
                Blocks.WATER.stateId,
                Blocks.LAVA.stateId,
            ).withIndex()
                .associate { it.value to it.index }
        val counts = IntArray(categories.size)
        var hash = 1L
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val category = categories[chunk.getBlockStateId(x, y, z)] ?: -1
                    hash = 31L * hash + category
                    if (category >= 0) counts[category]++
                }
            }
        }
        return hash to counts
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
            ): Int = 63

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

    private val stoneStateId = stateId("stone")

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing minecraft:$path")
}
