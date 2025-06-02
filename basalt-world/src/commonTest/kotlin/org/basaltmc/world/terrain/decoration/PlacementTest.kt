package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
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
