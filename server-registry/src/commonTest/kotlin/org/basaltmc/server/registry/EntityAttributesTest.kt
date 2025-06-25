package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `DefaultAttributes`, each mob's own `createAttributes`. */
class EntityAttributesTest {
    private fun mob(name: String) = Identifier.vanilla(name)

    @Test
    fun eachMobHasItsOwnHealthAndSpeed() {
        assertEquals(10.0, EntityAttributes.baseValue(mob("cow"), EntityAttributes.MAX_HEALTH))
        assertEquals(20.0, EntityAttributes.baseValue(mob("zombie"), EntityAttributes.MAX_HEALTH))
        assertEquals(3.0, EntityAttributes.baseValue(mob("rabbit"), EntityAttributes.MAX_HEALTH))
        assertEquals(
            0.2,
            EntityAttributes.baseValue(mob("cow"), EntityAttributes.MOVEMENT_SPEED) ?: 0.0,
            1e-6,
        )
        assertEquals(
            0.23,
            EntityAttributes.baseValue(mob("zombie"), EntityAttributes.MOVEMENT_SPEED) ?: 0.0,
            1e-6,
        )
    }

    @Test
    fun onlyWhatIsAliveHasAttributes() {
        assertTrue(EntityAttributes.isLiving(mob("cow")))
        assertTrue(EntityAttributes.isLiving(mob("player")))
        assertFalse(EntityAttributes.isLiving(mob("item")), "a dropped item is not alive")
        assertFalse(EntityAttributes.isLiving(mob("arrow")))
        assertNull(EntityAttributes.baseValue(mob("tnt"), EntityAttributes.MAX_HEALTH))
    }

    /** parity: `FOLLOW_RANGE`, which is how far a mob notices anything at all. */
    @Test
    fun aCreeperNoticesYouSixteenBlocksAway() {
        assertEquals(16.0, EntityAttributes.baseValue(mob("creeper"), EntityAttributes.FOLLOW_RANGE))
    }
}
