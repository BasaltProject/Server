package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import kotlin.test.Test
import kotlin.test.assertTrue

class PlacementTest {
    @Test
    fun airTargetUsesDimensionBoundsAndAcceptsCaveAir() {
        val chunk = Chunk(0, 0, ChunkHeight.NETHER)
        chunk.setBlockStateId(3, 200, 4, Blocks.CAVE_AIR.stateId)
        chunk.setBlockStateId(3, 201, 4, Blocks.VOID_AIR.stateId)

        assertTrue(
            AirTargetFilter(alsoCheckAbove = true).shouldPlace(
                PlacementContext(chunk),
                WorldgenRandom(LegacyRandom(0L)),
                DecorationPos(3, 200, 4),
            ),
        )
    }
}
