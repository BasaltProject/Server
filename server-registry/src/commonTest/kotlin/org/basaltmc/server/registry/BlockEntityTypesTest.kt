package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class BlockEntityTypesTest {
    /** Registry order is the protocol ID, so the ends and a couple of known middles pin the table. */
    @Test
    fun protocolIdsFollowRegistryOrder() {
        assertEquals(0, BlockEntityTypes.idOf(Identifier.vanilla("furnace")))
        assertEquals(1, BlockEntityTypes.idOf(Identifier.vanilla("chest")))
        assertEquals(20, BlockEntityTypes.idOf(Identifier.vanilla("banner")))
        // The one value this server carried hardcoded before the table existed.
        assertEquals(33, BlockEntityTypes.idOf(Identifier.vanilla("beehive")))
        assertEquals(BlockEntityTypes.size - 1, BlockEntityTypes.idOf(Identifier.vanilla("potent_sulfur")))
    }

    @Test
    fun anUnregisteredOrNonVanillaTypeHasNoId() {
        assertNull(BlockEntityTypes.idOf(Identifier.vanilla("not_a_block_entity")))
        assertNull(BlockEntityTypes.idOf(Identifier("basalt", "furnace")))
    }
}
