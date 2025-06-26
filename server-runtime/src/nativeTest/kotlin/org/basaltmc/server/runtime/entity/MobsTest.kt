package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** parity: `MobCategory` says where a type spawns, not whether it has a brain. */
class MobsTest {
    @Test
    fun theMiscCategoryStillHoldsMobs() {
        assertTrue(Mobs.isMob(Identifier.vanilla("villager")))
        assertTrue(Mobs.isMob(Identifier.vanilla("iron_golem")))
        assertTrue(Mobs.isMob(Identifier.vanilla("snow_golem")))
    }

    @Test
    fun theFurnitureIsStillNotAMob() {
        assertFalse(Mobs.isMob(Identifier.vanilla("item")))
        assertFalse(Mobs.isMob(Identifier.vanilla("arrow")))
        assertFalse(Mobs.isMob(Identifier.vanilla("armor_stand")))
        assertFalse(Mobs.isMob(Identifier.vanilla("minecart")))
    }

    @Test
    fun everythingThatSpawnsInAWaveIsAMob() {
        assertTrue(Mobs.isMob(Identifier.vanilla("zombie")))
        assertTrue(Mobs.isMob(Identifier.vanilla("cow")))
        assertTrue(Mobs.isMob(Identifier.vanilla("bat")))
    }
}
