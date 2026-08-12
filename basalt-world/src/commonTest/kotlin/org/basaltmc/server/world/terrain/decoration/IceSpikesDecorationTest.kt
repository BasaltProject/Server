package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class IceSpikesDecorationTest {
    @Test
    fun buildsPackedIceSpikesAndPatchesOnSnow() {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, 61, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, 62, z, Blocks.SNOW_BLOCK.stateId)
            }
        }

        IceSpikesDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        var packedIce = 0
        var highest = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 50..120) {
                    if (chunk.getBlockStateId(x, y, z) == Blocks.PACKED_ICE.stateId) {
                        packedIce++
                        highest = maxOf(highest, y)
                    }
                }
            }
        }
        assertTrue(packedIce > 0)
        assertTrue(highest > 62)
    }
}
