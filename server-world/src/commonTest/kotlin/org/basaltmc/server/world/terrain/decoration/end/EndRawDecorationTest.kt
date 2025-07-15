package org.basaltmc.server.world.terrain.decoration.end

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import kotlin.test.Test
import kotlin.test.assertTrue

class EndRawDecorationTest {
    @Test
    fun decoratedIslandCrossingChunkEdgeKeepsWorldCoordinates() {
        val chunk = Chunk(0, 0, ChunkHeight.END)

        EndRawDecoration.placeAt(
            chunk,
            WorldgenRandom(XoroshiroRandom(0L)),
            15,
            64,
            8,
        )

        val external = chunk.takeGeneratedBlockWrites()
        assertTrue(external.any { it.x >= 16 }, "expected the island to continue into the east chunk")
        assertTrue(external.all { it.expectedStateId == Blocks.AIR.stateId })
        assertTrue(external.all { it.stateId == Blocks.END_STONE.stateId })
    }
}
