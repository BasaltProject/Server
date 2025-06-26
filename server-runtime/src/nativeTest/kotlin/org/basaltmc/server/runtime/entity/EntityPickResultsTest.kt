package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** parity: `Entity.getPickResult`, which is what middle-clicking an entity answers with. */
class EntityPickResultsTest {
    private fun pick(type: String): String? = EntityPickResults.itemFor(Identifier.vanilla(type))?.path

    @Test
    fun aMobAnswersWithItsOwnSpawnEgg() {
        assertEquals("cow_spawn_egg", pick("cow"))
        assertEquals("zombie_spawn_egg", pick("zombie"))
        assertEquals("villager_spawn_egg", pick("villager"))
    }

    @Test
    fun whatWasPlacedFromAnItemAnswersWithThatItem() {
        assertEquals("minecart", pick("minecart"))
        assertEquals("chest_minecart", pick("chest_minecart"))
        assertEquals("oak_boat", pick("oak_boat"))
        assertEquals("bamboo_raft", pick("bamboo_raft"))
        assertEquals("armor_stand", pick("armor_stand"))
        assertEquals("painting", pick("painting"))
        assertEquals("lead", pick("leash_knot"))
    }

    @Test
    fun sharingANameWithAnItemIsNotEnough() {
        assertNull(pick("arrow"))
        assertNull(pick("snowball"))
        assertNull(pick("tnt"))
        assertNull(pick("item"))
    }

    @Test
    fun everyMobWithAnEggHasOne() {
        // 26.2 ships an egg for the two that used to have none, so both are pickable.
        assertEquals("iron_golem_spawn_egg", pick("iron_golem"))
        assertEquals("ender_dragon_spawn_egg", pick("ender_dragon"))
    }

    @Test
    fun aPlayerAnswersWithNothing() {
        assertNull(pick("player"))
    }
}
