package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class JungleVineDecorationTest {
    @Test
    fun placesWestFacingVinesBesideSolidColumns() {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16 step 2) {
            for (z in 0 until 16) {
                for (y in 64..100) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }

        JungleVineDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(31L)))

        val westFacingVineState = defaultVineStateId - 1
        var vines = 0
        for (x in 1 until 16 step 2) {
            for (z in 0 until 16) {
                for (y in 64..100) if (chunk.getBlockStateId(x, y, z) == westFacingVineState) vines++
            }
        }
        assertTrue(vines > 0)
    }

    private val defaultVineStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("vine"))?.stateId ?: error("unknown block 'vine'")
}
