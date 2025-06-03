package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import kotlin.test.Test
import kotlin.test.assertTrue

class BambooDecorationTest {
    @Test
    fun denseBambooPlacesStalksAndPodzolOnSupportedGround() {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y until 40) chunk.setBlockStateId(x, y, z, Blocks.DIRT.stateId)
                chunk.setBlockStateId(x, 40, z, Blocks.GRASS_BLOCK.stateId)
            }
        }

        BambooDecoration.DENSE.decorate(chunk, WorldgenRandom(XoroshiroRandom(1234L)))

        val states = (0 until chunk.sectionCount).flatMap { chunk.section(it).blockStateIds().asList() }
        assertTrue(states.count { it == Blocks.BAMBOO.stateId } > 20)
        assertTrue(states.any { it == Blocks.PODZOL.stateId })
    }
}
