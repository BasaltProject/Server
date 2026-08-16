package org.basaltmc.server.runtime.world

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class RealFluidTagsTest {
    /** parity: the `minecraft:fluid` registry ids, which is what a fluid tag carries on the wire. */
    @Test
    fun theFluidIdsAreTheOnesTheClientResolvesAgainst() {
        assertEquals(1, RealFluidTags.PROTOCOL_IDS[Identifier.vanilla("flowing_water")])
        assertEquals(2, RealFluidTags.PROTOCOL_IDS[Identifier.vanilla("water")])
        assertEquals(3, RealFluidTags.PROTOCOL_IDS[Identifier.vanilla("flowing_lava")])
        assertEquals(4, RealFluidTags.PROTOCOL_IDS[Identifier.vanilla("lava")])
    }

    @Test
    fun withoutTheJarNothingIsTagged() {
        val tags = RealFluidTags(serverJar = null)

        assertEquals(0, tags.size)
        assertTrue(tags.membersOf(Identifier.vanilla("water")).isEmpty())
    }
}
