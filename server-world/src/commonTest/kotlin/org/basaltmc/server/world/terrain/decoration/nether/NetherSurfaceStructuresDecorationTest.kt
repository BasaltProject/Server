package org.basaltmc.server.world.terrain.decoration.nether

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.terrain.NetherBiomeSource
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class NetherSurfaceStructuresDecorationTest {
    @Test
    fun deltaAndColumnsAreDeterministicAndBiomeFiltered() {
        val first = layeredChunk()
        val second = layeredChunk()
        val wastes = layeredChunk()
        val columns = openFloorChunk()

        NetherSurfaceStructuresDecoration.decorate(first, 1234L, source(-0.5, 0.0))
        NetherSurfaceStructuresDecoration.decorate(second, 1234L, source(-0.5, 0.0))
        NetherSurfaceStructuresDecoration.decorate(wastes, 1234L, source(0.0, 0.0))
        NetherSurfaceStructuresDecoration.decorateFeature(columns, 1234L, source(-0.5, 0.0), 1)

        assertContentEquals(states(first), states(second))
        assertTrue(count(first, "magma_block") > 0)
        assertTrue(count(first, "lava") > 0)
        assertTrue(count(columns, "basalt") > 0)
        assertEquals(0, count(wastes, "magma_block"))
        assertEquals(0, count(wastes, "lava"))
        assertEquals(0, count(wastes, "basalt"))
    }

    private fun layeredChunk(): Chunk =
        Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (y in 0..127) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, Blocks.NETHERRACK.stateId)
                }
            }
            for (z in 1 until 15) {
                for (x in 1 until 15) {
                    if ((x + z) and 1 != 0) continue
                    for (y in 1..127) chunk.setBlockStateId(x, y, z, Blocks.AIR.stateId)
                }
            }
        }

    private fun openFloorChunk(): Chunk =
        Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (y in 0..40) {
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
