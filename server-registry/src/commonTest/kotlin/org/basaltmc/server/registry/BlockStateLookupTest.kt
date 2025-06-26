package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

/** What `/execute if block` and every interaction ask: which block is this, and how is it set. */
class BlockStateLookupTest {
    @Test
    fun aDefaultStateStillKnowsItsOwnProperties() {
        val torch = checkNotNull(Blocks.REGISTRY.idOf(Identifier.vanilla("redstone_torch")))
        val state = assertNotNull(Blocks.byStateId(torch))
        assertEquals(Identifier.vanilla("redstone_torch"), state.block)
        assertEquals("true", state.properties["lit"], "a torch's default state is a lit one")
    }

    @Test
    fun aStateAwayFromTheDefaultReadsToo() {
        val delayed =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(Identifier.vanilla("repeater"), mapOf("delay" to "3")),
            )
        val state = assertNotNull(Blocks.byStateId(delayed))
        assertEquals("3", state.properties["delay"])
    }

    @Test
    fun aBlockWithNoPropertiesAtAllStillResolves() {
        val state = assertNotNull(Blocks.byStateId(Blocks.STONE.stateId))
        assertEquals(Identifier.vanilla("stone"), state.block)
    }
}
