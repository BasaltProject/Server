package org.basaltmc.server.runtime.item

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ItemWearTest {
    private val random = LegacyRandom(1L)

    private fun pickaxe(
        damage: Int? = null,
        unbreakable: Boolean = false,
    ): StoredItemStack {
        val components = NbtCompound()
        damage?.let { components["minecraft:damage"] = NbtInt(it) }
        if (unbreakable) components["minecraft:unbreakable"] = NbtCompound()
        return StoredItemStack("minecraft:diamond_pickaxe", 1, components)
    }

    @Test
    fun aToolTakesOnePointPerBlock() {
        val worn = checkNotNull(ItemWear.hurt(pickaxe(), ItemWear.costPerBlock(pickaxe()), random))
        assertEquals(1, ItemWear.damage(worn))
    }

    @Test
    fun theLastPointBreaksItRatherThanLeavingItAtZero() {
        val nearlyGone = pickaxe(damage = ItemWear.maxDamage(pickaxe()) - 1)
        assertNull(ItemWear.hurt(nearlyGone, 1, random))
    }

    @Test
    fun anUnbreakableToolNeverWears() {
        val stack = pickaxe(unbreakable = true)
        assertFalse(ItemWear.isDamageable(stack))
        assertEquals(stack, ItemWear.hurt(stack, 1, random))
    }

    @Test
    fun anItemWithNoDurabilityIsLeftAlone() {
        val stack = StoredItemStack("minecraft:stone", 3)
        assertFalse(ItemWear.isDamageable(stack))
        assertEquals(stack, ItemWear.hurt(stack, 1, random))
    }

    @Test
    fun armourTakesNoWearFromMining() {
        // parity: `Item.mineBlock` returns early without a `minecraft:tool` component, so a helmet
        // in hand is not a pickaxe with a slower wear rate: it does not wear at all.
        assertEquals(0, ItemWear.costPerBlock(StoredItemStack("minecraft:diamond_helmet", 1)))
        assertTrue(ItemWear.isDamageable(StoredItemStack("minecraft:diamond_helmet", 1)))
    }

    @Test
    fun aSwordCostsTwoPerBlockAndOnePerAttack() {
        val sword = StoredItemStack("minecraft:diamond_sword", 1)
        assertEquals(2, ItemWear.costPerBlock(sword))
        assertEquals(1, ItemWear.costPerAttack(sword))
    }
}
