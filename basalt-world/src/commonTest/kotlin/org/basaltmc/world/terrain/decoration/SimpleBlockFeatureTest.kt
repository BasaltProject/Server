package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals

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
}
