package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class SimpleBlockFeatureTest {
    @Test
    fun stateProviderRunsBeforeFailedSurvivalCheck() {
        val actual = WorldgenRandom(XoroshiroRandom(91L))
        val expected = WorldgenRandom(XoroshiroRandom(91L))
        val feature =
            SimpleBlockFeature({ random, _ ->
                random.nextInt(16)
                Blocks.GRASS_BLOCK.stateId
            })

        feature.place(Chunk(0, 0), actual, DecorationPos(0, 63, 0))
        expected.nextInt(16)

        assertEquals(expected.nextLong(), actual.nextLong())
    }

    @Test
    fun sequentialExternalWritesRemainAtTheirWorldCoordinates() {
        val chunk = Chunk(0, 0)
        chunk.setDecorationBlockStateId(16, 62, 4, Blocks.GRASS_BLOCK.stateId)

        assertTrue(
            SimpleBlockFeature({ _, _ -> Blocks.OAK_LOG.stateId })
                .place(chunk, WorldgenRandom(XoroshiroRandom(1L)), DecorationPos(16, 63, 4)),
        )

        val writes = chunk.takeGeneratedBlockWrites()
        assertEquals(Blocks.GRASS_BLOCK.stateId, writes.single { it.y == 62 }.stateId)
        assertEquals(Blocks.OAK_LOG.stateId, writes.single { it.y == 63 }.stateId)
        assertTrue(writes.all { it.x == 16 && it.z == 4 })
    }
}
