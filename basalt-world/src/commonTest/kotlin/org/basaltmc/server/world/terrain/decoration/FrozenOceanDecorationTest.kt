package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class FrozenOceanDecorationTest {
    @Test
    fun icebergBuildsPackedIceAboveAndBelowSeaLevel() {
        val chunk = oceanChunk()
        val region = ChunkFeatureRegion(chunk)

        IcebergFeature.place(
            region,
            WorldgenRandom(XoroshiroRandom(1234L)),
            DecorationPos(8, 0, 8),
            Blocks.PACKED_ICE.stateId,
        )
        region.flush()

        assertTrue(countState(chunk, Blocks.PACKED_ICE.stateId, 63, 100) > 0)
        assertTrue(countState(chunk, Blocks.PACKED_ICE.stateId, 30, 62) > 0)
    }

    @Test
    fun blueIceSpreadsFromWaterAdjacentToPackedIce() {
        val chunk = oceanChunk()
        chunk.setBlockStateId(9, 50, 8, Blocks.PACKED_ICE.stateId)
        val region = ChunkFeatureRegion(chunk)

        val placed = BlueIceFeature.place(region, WorldgenRandom(XoroshiroRandom(99L)), DecorationPos(8, 50, 8))
        region.flush()

        assertTrue(placed)
        assertEquals(Blocks.BLUE_ICE.stateId, chunk.getBlockStateId(8, 50, 8))
        assertTrue(countState(chunk, Blocks.BLUE_ICE.stateId, 30, 62) > 1)
    }

    private fun oceanChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 30..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                }
            }
        }

    private fun countState(
        chunk: Chunk,
        stateId: Int,
        minY: Int,
        maxY: Int,
    ): Int {
        var count = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in minY..maxY) if (chunk.getBlockStateId(x, y, z) == stateId) count++
            }
        }
        return count
    }
}
