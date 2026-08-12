package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.terrain.EndNoiseRouter
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import org.basaltmc.wire.nbt.NbtByte
import org.basaltmc.wire.nbt.NbtIntArray
import org.basaltmc.wire.nbt.NbtLong
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

class EndSurfaceStructuresDecorationTest {
    @Test
    fun returnGatewayMatchesOfficial26_2Vector() {
        val chunk = flatEndStoneChunk(65, 0)
        val router = EndNoiseRouter(ConstantDensity(1.0), ConstantDensity(-1.0))

        EndSurfaceStructuresDecoration.decorate(chunk, 355L, router, TheEndBiomeSource(router.erosion))

        assertEquals(12, count(chunk, Blocks.BEDROCK.stateId))
        assertEquals(1, count(chunk, Blocks.END_GATEWAY.stateId))
        assertEquals(Blocks.END_GATEWAY.stateId, chunk.getBlockStateId(13, 72, 8))
        assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(13, 70, 8))
        assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(12, 71, 8))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(12, 72, 8))

        val entity = assertNotNull(chunk.blockEntityAt(13, 72, 8))
        assertEquals("minecraft:end_gateway", entity.type.toString())
        assertEquals(NbtLong(0L), entity.data["Age"])
        assertEquals(NbtByte.TRUE, entity.data["ExactTeleport"])
        assertContentEquals(intArrayOf(100, 50, 0), (entity.data["exit_portal"] as NbtIntArray).value)
        assertNotNull(entity.data["components"])
    }

    @Test
    fun returnGatewayReplaysWritesFromNeighborSourceChunk() {
        val target = Chunk(64, 0, ChunkHeight.END)
        val router = EndNoiseRouter(ConstantDensity(1.0), FlatEndDensity)

        EndSurfaceStructuresDecoration.decorate(target, 1680L, router, TheEndBiomeSource(router.erosion))

        assertEquals(Blocks.BEDROCK.stateId, target.getBlockStateId(15, 67, 2))
        assertEquals(Blocks.BEDROCK.stateId, target.getBlockStateId(15, 69, 2))
        assertEquals(2, count(target, Blocks.BEDROCK.stateId))
        assertEquals(0, count(target, Blocks.END_GATEWAY.stateId))
        assertEquals(0, target.blockEntities().size)
    }

    @Test
    fun returnGatewayRespectsRarityFilter() {
        val chunk = flatEndStoneChunk(65, 0)
        val router = EndNoiseRouter(ConstantDensity(1.0), ConstantDensity(-1.0))

        EndSurfaceStructuresDecoration.decorate(chunk, 0L, router, TheEndBiomeSource(router.erosion))

        assertEquals(0, count(chunk, Blocks.END_GATEWAY.stateId))
    }

    private fun flatEndStoneChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk =
        Chunk(chunkX, chunkZ, ChunkHeight.END).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) chunk.setBlockStateId(x, 64, z, Blocks.END_STONE.stateId)
            }
        }

    private fun count(
        chunk: Chunk,
        stateId: Int,
    ): Int {
        var result = 0
        for (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) if (chunk.getBlockStateId(x, y, z) == stateId) result++
            }
        }
        return result
    }

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }

    private data object FlatEndDensity : DensityFunction {
        override fun compute(context: FunctionContext): Double = if (context.blockY <= 64) 0.08 else -0.72

        override fun minValue(): Double = -0.72

        override fun maxValue(): Double = 0.08
    }
}
