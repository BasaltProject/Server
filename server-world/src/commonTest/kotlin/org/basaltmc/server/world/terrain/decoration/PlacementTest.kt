package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
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

    @Test
    fun airTargetReadsExternalDecorationPositionsWithoutWrapping() {
        val chunk = Chunk(0, 0)
        val context = PlacementContext(chunk)
        val random = WorldgenRandom(LegacyRandom(0L))
        chunk.setBlockStateId(0, 64, 0, Blocks.STONE.stateId)

        assertTrue(AirTargetFilter().shouldPlace(context, random, DecorationPos(16, 64, 0)))

        chunk.setDecorationBlockStateId(16, 64, 0, Blocks.STONE.stateId)
        assertFalse(AirTargetFilter().shouldPlace(context, random, DecorationPos(16, 64, 0)))
    }

    @Test
    fun externalDecorationWritesKeepTheFirstObservedDeliveryGuard() {
        val chunk = Chunk(0, 0)
        val position = DecorationPos(16, 40, 4)
        val featureView = HashMap<DecorationPos, Int>()
        val neighbourState = { _: Int, _: Int, _: Int -> Blocks.GRASS_BLOCK.stateId }

        setExternalDecorationBlockStateId(chunk, featureView, neighbourState, position, Blocks.PODZOL.stateId)
        setExternalDecorationBlockStateId(chunk, featureView, neighbourState, position, Blocks.BAMBOO.stateId)

        assertEquals(Blocks.BAMBOO.stateId, featureView[position])
        val write = chunk.takeGeneratedBlockWrites().single()
        assertEquals(Blocks.GRASS_BLOCK.stateId, write.expectedStateId)
        assertEquals(Blocks.BAMBOO.stateId, write.stateId)
    }
}
