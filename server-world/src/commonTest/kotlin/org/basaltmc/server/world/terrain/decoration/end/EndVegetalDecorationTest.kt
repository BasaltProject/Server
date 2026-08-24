package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.terrain.EndNoiseRouter
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class EndVegetalDecorationTest {
    @Test
    fun chorusPlantMatchesOfficial26_2Vector() {
        val chunk = flatEndStoneChunk(65, 0)
        val router = EndNoiseRouter(ConstantDensity(1.0), ConstantDensity(-1.0))

        EndVegetalDecoration.decorate(chunk, 0L, router, TheEndBiomeSource(router.erosion))

        var plants = 0
        var flowers = 0
        var checksum = 0L
        for (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    when (Blocks.byStateId(stateId)?.block?.path) {
                        "chorus_plant" -> plants++
                        "chorus_flower" -> flowers++
                        else -> continue
                    }
                    checksum =
                        checksum * 31 +
                        (chunk.chunkX * 16 + x) * 73428767L +
                        y * 912931L +
                        (chunk.chunkZ * 16 + z) * 438289L +
                        stateId
                }
            }
        }

        assertEquals(11, plants)
        assertEquals(1, flowers)
        assertEquals(-5965029896630212221L, checksum)
        assertEquals(14671, chunk.getBlockStateId(11, 65, 1))
        assertEquals(14711, chunk.getBlockStateId(12, 74, 0))
    }

    @Test
    fun chorusPlantRespectsTheEndHighlandsBiomeFilter() {
        val chunk = flatEndStoneChunk(65, 0)
        val router = EndNoiseRouter(ConstantDensity(-1.0), ConstantDensity(-1.0))

        EndVegetalDecoration.decorate(chunk, 0L, router, TheEndBiomeSource(router.erosion))

        assertEquals(0, count(chunk, "chorus_plant") + count(chunk, "chorus_flower"))
    }

    @Test
    fun chorusPlantCrossingChunkEdgeKeepsWorldCoordinates() {
        val router = EndNoiseRouter(ConstantDensity(-1.0), ConstantDensity(-1.0))
        val chunk = flatEndStoneChunk(0, 0)
        assertTrue(
            EndVegetalDecoration.placeAt(
                chunk,
                WorldgenRandom(XoroshiroRandom(0L)),
                router,
                15,
                65,
                8,
            ),
        )
        val crossingWrites = chunk.takeGeneratedBlockWrites()

        assertTrue(crossingWrites.any { it.x >= 16 }, "expected chorus to continue into the east chunk")
        assertTrue(crossingWrites.all { it.expectedStateId == Blocks.AIR.stateId })
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
        path: String,
    ): Int {
        var count = 0
        for (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    if (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path == path) count++
                }
            }
        }
        return count
    }

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
