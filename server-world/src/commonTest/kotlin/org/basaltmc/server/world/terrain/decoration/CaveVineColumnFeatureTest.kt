package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class CaveVineColumnFeatureTest {
    @Test
    fun columnKeepsPlantBodiesAndAgedTip() {
        var verifiedBody = false
        for (seed in 0L..1_000L) {
            val chunk = Chunk(0, 0)
            CaveVineColumnFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), DecorationPos(8, 50, 8))
            val states =
                (WorldHeightRange)
                    .map { y -> chunk.getBlockStateId(8, y, 8) }
                    .filterNot(Blocks::isAir)
            if (states.size < 2) continue

            val tipProperties = Blocks.byStateId(states.first())?.properties ?: error("missing tip properties")
            assertTrue(tipProperties["age"]?.toIntOrNull() in 23..25)
            assertTrue(tipProperties["berries"] in setOf("true", "false"))
            assertTrue(
                states.drop(1).all { state ->
                    Blocks.byStateId(state)?.block?.path == "cave_vines_plant" &&
                        Blocks.byStateId(state)?.properties?.get("berries") in setOf("true", "false")
                },
            )
            verifiedBody = true
            break
        }
        assertTrue(verifiedBody, "expected a sampled column with body and tip")
    }

    @Test
    fun obstructionBelowOriginTruncatesThePrioritizedTipToo() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(8, 49, 8, Blocks.STONE.stateId)
        CaveVineColumnFeature.place(chunk, WorldgenRandom(XoroshiroRandom(1234L)), DecorationPos(8, 50, 8))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(8, 50, 8))
    }

    private companion object {
        val WorldHeightRange = 30..50
    }
}
