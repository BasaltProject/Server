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

class NetherLocalDecorationTest {
    @Test
    fun basaltPillarsAreDeterministicAndSoulValleyOnly() {
        val first = layeredChunk()
        val second = layeredChunk()
        val wastes = layeredChunk()

        NetherLocalDecoration.decorate(first, 1234L, source(0.0, -0.5))
        NetherLocalDecoration.decorate(second, 1234L, source(0.0, -0.5))
        NetherLocalDecoration.decorate(wastes, 1234L, source(0.0, 0.0))

        assertContentEquals(states(first), states(second))
        assertTrue(count(first, "basalt") > 0)
        assertEquals(0, count(wastes, "basalt"))
    }

    private fun layeredChunk(): Chunk =
        Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (y in 0..127 step 4) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, Blocks.NETHERRACK.stateId)
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
    ): Int {
        val stateId =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(blockPath))?.stateId
                ?: error("missing vanilla block minecraft:$blockPath")
        return states(chunk).count { it == stateId }
    }

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
