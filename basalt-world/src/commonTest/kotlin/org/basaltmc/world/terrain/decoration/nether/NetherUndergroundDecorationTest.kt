package org.basaltmc.world.terrain.decoration.nether

import org.basaltmc.common.Identifier
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.terrain.NetherBiomeSource
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class NetherUndergroundDecorationTest {
    @Test
    fun undergroundStepIsDeterministicAndRunsNonOreFeatures() {
        val first = layeredChunk("netherrack")
        val second = layeredChunk("netherrack")
        val biomeSource = source(-0.5, 0.0)

        NetherUndergroundDecoration.decorate(first, 1234L, biomeSource)
        NetherUndergroundDecoration.decorate(second, 1234L, biomeSource)

        assertContentEquals(states(first), states(second))
        assertTrue(count(first, "basalt") > 0)
        assertTrue(count(first, "blackstone") > 0)
        assertTrue(count(first, "fire") > 0)
        assertTrue(count(first, "glowstone") > 0)
    }

    @Test
    fun soulValleyPatchUsesSoulSoilSupport() {
        val chunk = layeredChunk("soul_soil")

        NetherUndergroundDecoration.decorate(chunk, 9876L, source(0.0, -0.5))

        assertTrue(count(chunk, "soul_fire") > 0)
        assertTrue(count(chunk, "crimson_roots") > 0)
    }

    @Test
    fun closedSpringUsesItsGlobalStreamAndBiomeFilter() {
        val wastes = solidChunk("netherrack")
        val deltaSkipped = solidChunk("netherrack")
        val delta = solidChunk("netherrack")

        NetherUndergroundDecoration.decorateFeature(wastes, 42L, source(0.0, 0.0), 15)
        NetherUndergroundDecoration.decorateFeature(deltaSkipped, 42L, source(-0.5, 0.0), 15)
        NetherUndergroundDecoration.decorateFeature(delta, 42L, source(-0.5, 0.0), 12)

        assertTrue(count(wastes, "lava") > 0)
        assertEquals(0, count(deltaSkipped, "lava"))
        assertTrue(count(delta, "lava") > 0)
    }

    private fun layeredChunk(blockPath: String): Chunk {
        val stateId = stateId(blockPath)
        return Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (y in 0..127 step 4) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, stateId)
                }
            }
        }
    }

    private fun solidChunk(blockPath: String): Chunk {
        val stateId = stateId(blockPath)
        return Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (y in 0..127) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, stateId)
                }
            }
        }
    }

    private fun source(
        temperature: Double,
        vegetation: Double,
    ): NetherBiomeSource = NetherBiomeSource(ConstantDensity(temperature), ConstantDensity(vegetation))

    private fun states(chunk: Chunk): IntArray =
        IntArray(128 * 16 * 16) { index ->
            val y = index / 256
            val remainder = index % 256
            chunk.getBlockStateId(remainder % 16, y, remainder / 16)
        }

    private fun count(
        chunk: Chunk,
        blockPath: String,
    ): Int = states(chunk).count { it == stateId(blockPath) }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
