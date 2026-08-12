package org.basaltmc.server.world.terrain.carver

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class NetherCaveCarverTest {
    @Test
    fun carvesDeterministicCaveAirAndLavaWithoutTouchingTheProtectedRoof() {
        val first = solidChunk()
        val second = solidChunk()

        NetherCaveCarver.carve(first, 1234L)
        NetherCaveCarver.carve(second, 1234L)

        assertContentEquals(states(first), states(second))
        assertTrue(count(first, Blocks.CAVE_AIR.stateId) > 0)
        assertTrue(count(first, Blocks.LAVA.stateId) > 0)
        for (y in 121..127) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    assertEquals(Blocks.NETHERRACK.stateId, first.getBlockStateId(x, y, z))
                }
            }
        }
    }

    private fun solidChunk(): Chunk =
        Chunk(0, 0, ChunkHeight.NETHER).also { chunk ->
            for (y in 0..127) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, Blocks.NETHERRACK.stateId)
                }
            }
        }

    private fun states(chunk: Chunk): IntArray =
        IntArray(128 * 16 * 16) { index ->
            val y = index / 256
            val remainder = index % 256
            chunk.getBlockStateId(remainder % 16, y, remainder / 16)
        }

    private fun count(
        chunk: Chunk,
        stateId: Int,
    ): Int = states(chunk).count { it == stateId }
}
