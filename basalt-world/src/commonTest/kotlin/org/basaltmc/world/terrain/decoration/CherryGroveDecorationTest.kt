package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class CherryGroveDecorationTest {
    @Test
    fun pinkPetalsUseTheirWeightedFacingAndAmountStates() {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }

        CherryGroveDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(42L)))

        val states = mutableSetOf<Int>()
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                val stateId = chunk.getBlockStateId(x, 63, z)
                if (Blocks.isPinkPetalsState(stateId)) states += stateId
            }
        }
        assertTrue(states.isNotEmpty())
        assertTrue(states.size > 1)
        assertTrue(
            states.all { stateId ->
                val properties = Blocks.byStateId(stateId)?.properties ?: return@all false
                properties["facing"] in setOf("north", "east", "south", "west") &&
                    properties["flower_amount"]?.toIntOrNull()?.let { it in 1..4 } == true
            },
        )
    }
}
