package org.basaltmc.registry

import kotlin.test.Test
import kotlin.test.assertEquals

class BlocksTest {
    @Test
    fun exposesTheExpectedStateIds() {
        assertEquals(0, Blocks.AIR.stateId)
        assertEquals(1, Blocks.STONE.stateId)
        assertEquals(9, Blocks.GRASS_BLOCK.stateId)
        assertEquals(10, Blocks.DIRT.stateId)
        assertEquals(85, Blocks.BEDROCK.stateId)
    }

    @Test
    fun blocksAreFoundByIdentifierInTheSharedRegistry() {
        assertEquals(Blocks.STONE, Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone")))
        assertEquals(Blocks.STONE, Blocks.REGISTRY.byId(1))
    }
}
