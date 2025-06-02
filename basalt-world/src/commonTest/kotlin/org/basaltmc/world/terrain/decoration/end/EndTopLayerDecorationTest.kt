package org.basaltmc.world.terrain.decoration.end

import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext
import org.basaltmc.world.terrain.TheEndBiomeSource
import kotlin.test.Test
import kotlin.test.assertEquals

class EndTopLayerDecorationTest {
    @Test
    fun endPlatformMatchesFixedVanillaGeometryAcrossChunkBorder() {
        val biomeSource = TheEndBiomeSource(ConstantDensity(0.0))
        val northChunk = filledChunk(6, -1)
        val southChunk = filledChunk(6, 0)

        EndTopLayerDecoration.decorate(northChunk, biomeSource)
        EndTopLayerDecoration.decorate(southChunk, biomeSource)

        assertPlatformSlice(northChunk, expectedRows = 2)
        assertPlatformSlice(southChunk, expectedRows = 3)
        assertEquals(Blocks.OBSIDIAN.stateId, northChunk.getBlockStateId(4, 48, 14))
        assertEquals(Blocks.AIR.stateId, northChunk.getBlockStateId(4, 51, 15))
        assertEquals(Blocks.OBSIDIAN.stateId, southChunk.getBlockStateId(4, 48, 0))
        assertEquals(Blocks.AIR.stateId, southChunk.getBlockStateId(6, 51, 2))
    }

    @Test
    fun endPlatformDoesNotModifyOtherChunks() {
        val chunk = filledChunk(5, 0)

        EndTopLayerDecoration.decorate(chunk, TheEndBiomeSource(ConstantDensity(0.0)))

        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(15, 48, 0))
        assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(15, 51, 0))
    }

    private fun assertPlatformSlice(
        chunk: Chunk,
        expectedRows: Int,
    ) {
        var obsidian = 0
        var air = 0
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                if (chunk.getBlockStateId(x, 48, z) == Blocks.OBSIDIAN.stateId) obsidian++
                for (y in 49..51) {
                    if (chunk.getBlockStateId(x, y, z) == Blocks.AIR.stateId) air++
                }
            }
        }
        assertEquals(expectedRows * 5, obsidian)
        assertEquals(expectedRows * 5 * 3, air)
    }

    private fun filledChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        Chunk(chunkX, chunkZ, ChunkHeight.END).also { chunk ->
            for (y in 48..51) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
                }
            }
        }

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
