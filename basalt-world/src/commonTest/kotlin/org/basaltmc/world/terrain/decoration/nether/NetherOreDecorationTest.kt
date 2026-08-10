package org.basaltmc.world.terrain.decoration.nether

import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.densityfunction.DensityFunctions
import org.basaltmc.world.terrain.NetherBiomeSource
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class NetherOreDecorationTest {
    @Test
    fun netherWastesOresAreDeterministicAndBiomeFiltered() {
        val first = solidChunk()
        val second = solidChunk()
        val biomeSource = NetherBiomeSource(DensityFunctions.zero(), DensityFunctions.zero())

        NetherOreDecoration.decorate(first, 1234L, biomeSource)
        NetherOreDecoration.decorate(second, 1234L, biomeSource)

        assertContentEquals(states(first), states(second))
        assertTrue(count(first, "magma_block") > 0)
        assertTrue(count(first, "gravel") > 0)
        assertTrue(count(first, "blackstone") > 0)
        assertTrue(count(first, "nether_gold_ore") > 0)
        assertTrue(count(first, "nether_quartz_ore") > 0)
        assertEquals(0, count(first, "soul_sand"))
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
        blockPath: String,
    ): Int {
        val state = Blocks.REGISTRY.byIdentifier(Identifier.vanilla(blockPath))
        val stateId = state?.stateId ?: error("missing $blockPath")
        return states(chunk).count { it == stateId }
    }
}
