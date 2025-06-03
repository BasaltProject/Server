package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.terrain.EndNoiseRouter
import org.basaltmc.server.world.terrain.TheEndBiomeSource
import kotlin.test.Test
import kotlin.test.assertEquals

class EndSpikeDecorationTest {
    @Test
    fun seedZeroLayoutMatchesOfficial26_2Vector() {
        val spikes = EndSurfaceStructuresDecoration.spikesForSeed(0L)

        assertEquals(
            listOf(
                "42,0|4|94|false",
                "33,24|3|91|false",
                "12,39|2|79|true",
                "-13,39|4|97|false",
                "-34,24|3|85|false",
                "-42,-1|5|103|false",
                "-34,-25|2|76|false",
                "-13,-40|2|82|true",
                "12,-40|3|88|false",
                "33,-25|4|100|false",
            ),
            spikes.map { "${it.centerX},${it.centerZ}|${it.radius}|${it.height}|${it.guarded}" },
        )
    }

    @Test
    fun unguardedSpikeMatchesOfficialBlocksAndCrystal() {
        val chunk = decorate(2, 0)

        assertEquals(33 * 94, count(chunk, Blocks.OBSIDIAN.stateId))
        assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(10, 94, 0))
        assertEquals(Blocks.FIRE.stateId, chunk.getBlockStateId(10, 95, 0))
        val crystal = chunk.generatedEntities().single()
        assertEquals("minecraft:end_crystal", crystal.type.toString())
        assertEquals(42.5, crystal.x)
        assertEquals(95.0, crystal.y)
        assertEquals(0.5, crystal.z)
        assertEquals(138.91124f, crystal.yaw)
        assertEquals(0.0f, crystal.pitch)
    }

    @Test
    fun guardedSpikeMatchesOfficialCageStates() {
        val chunk = decorate(0, 2)

        assertEquals(21 * 79, count(chunk, Blocks.OBSIDIAN.stateId))
        assertEquals(73, countPath(chunk, "iron_bars"))
        assertEquals(
            mapOf(
                7960 to 9,
                7961 to 3,
                7964 to 3,
                7965 to 4,
                7968 to 3,
                7969 to 4,
                7972 to 18,
                7976 to 3,
                7977 to 18,
                7980 to 4,
                7984 to 4,
            ),
            stateCounts(chunk, "iron_bars"),
        )
        assertEquals(Blocks.BEDROCK.stateId, chunk.getBlockStateId(12, 79, 7))
        assertEquals(Blocks.FIRE.stateId, chunk.getBlockStateId(12, 80, 7))
        assertEquals(155.42078f, chunk.generatedEntities().single().yaw)
    }

    @Test
    fun spikeReplaysWritesFromNeighborCenterChunk() {
        val chunk = decorate(1, 1)

        assertEquals(Blocks.OBSIDIAN.stateId, chunk.getBlockStateId(15, 0, 8))
        assertEquals(0, chunk.generatedEntities().size)
    }

    private fun decorate(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ, ChunkHeight.END)
        val router = EndNoiseRouter(ConstantDensity(0.0), ConstantDensity(-1.0))
        EndSurfaceStructuresDecoration.decorate(chunk, 0L, router, TheEndBiomeSource(router.erosion))
        return chunk
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

    private fun countPath(
        chunk: Chunk,
        path: String,
    ): Int = stateCounts(chunk, path).values.sum()

    private fun stateCounts(
        chunk: Chunk,
        path: String,
    ): Map<Int, Int> {
        val result = mutableMapOf<Int, Int>()
        for (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    if (Blocks.byStateId(stateId)?.block?.path == path) result[stateId] = (result[stateId] ?: 0) + 1
                }
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
}
