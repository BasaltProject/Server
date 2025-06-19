package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.item.ItemWear
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtString
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** parity: `AnvilMenu.createResult`. */
class AnvilsTest {
    private fun stack(
        id: String,
        count: Int = 1,
        damage: Int? = null,
        repairCost: Int? = null,
        name: String? = null,
    ): StoredItemStack {
        val components = NbtCompound()
        damage?.let { components[ItemComponentCodecs.DAMAGE] = NbtInt(it) }
        repairCost?.let { components[ItemComponentCodecs.REPAIR_COST] = NbtInt(it) }
        name?.let { components[ItemComponentCodecs.CUSTOM_NAME] = NbtString(it) }
        return StoredItemStack("minecraft:$id", count, components)
    }

    /** parity: a quarter of the maximum per ingot, and a level for each ingot it actually used. */
    @Test
    fun ironIngotsRepairAnIronPickaxe() {
        val pickaxe = stack("iron_pickaxe", damage = 200)
        val max = ItemWear.maxDamage(pickaxe)

        val result =
            Anvils.combine(pickaxe, stack("iron_ingot", count = 3), isRepairMaterial = Anvils::isRepairMaterial)

        val output = assertNotNull(result.output)
        // A quarter of 250 is 62 a time, and vanilla keeps going while there is damage and metal.
        assertEquals(200 - (max / 4) * 3, ItemWear.damage(output), "three ingots' worth came off")
        assertEquals(3, result.cost, "a level for each one used")
        assertEquals(3, result.materialCount)
    }

    /** parity: an undamaged item takes no repair, and the anvil offers nothing. */
    @Test
    fun anUndamagedToolIsNotRepaired() {
        val result =
            Anvils.combine(stack("iron_pickaxe"), stack("iron_ingot"), isRepairMaterial = Anvils::isRepairMaterial)

        assertNull(result.output)
        assertEquals(0, result.cost)
    }

    /** parity: two of the same tool add their durability plus twelve per cent, for two levels. */
    @Test
    fun twoPickaxesCombine() {
        val max = ItemWear.maxDamage(stack("iron_pickaxe"))
        val result = Anvils.combine(stack("iron_pickaxe", damage = max - 10), stack("iron_pickaxe", damage = max - 10))

        val output = assertNotNull(result.output)
        assertEquals(max - (10 + 10 + max * 12 / 100), ItemWear.damage(output))
        assertEquals(2, result.cost)
    }

    /** parity: renaming is one level, and the prior work is not doubled for a rename alone. */
    @Test
    fun renamingCostsOneAndDoesNotRaiseThePriorWork() {
        val result = Anvils.combine(stack("iron_pickaxe", repairCost = 3), null, newName = "Diggy")

        val output = assertNotNull(result.output)
        assertEquals(4, result.cost, "three of prior work plus the level for the name")
        assertEquals("Diggy", (output.components[ItemComponentCodecs.CUSTOM_NAME] as? NbtString)?.value)
        assertEquals(3, Anvils.repairCost(output), "a rename alone leaves the prior work where it was")
    }

    /** parity: `calculateIncreasedRepairCost`, which is what makes an item run out of anvil. */
    @Test
    fun priorWorkDoublesAndAddsOne() {
        assertEquals(1, Anvils.increasedRepairCost(0))
        assertEquals(3, Anvils.increasedRepairCost(1))
        assertEquals(7, Anvils.increasedRepairCost(3))
        assertEquals(31, Anvils.increasedRepairCost(15))

        val max = ItemWear.maxDamage(stack("iron_pickaxe"))
        val result =
            Anvils.combine(
                stack("iron_pickaxe", damage = 100, repairCost = 3),
                stack("iron_ingot"),
                isRepairMaterial = Anvils::isRepairMaterial,
            )
        assertEquals(7, Anvils.repairCost(assertNotNull(result.output)), "and a real repair does double it")
        assertEquals(max, ItemWear.maxDamage(stack("iron_pickaxe")))
    }

    /** parity: forty levels is where survival gives up. */
    @Test
    fun fortyLevelsIsTooExpensive() {
        val result =
            Anvils.combine(
                stack("iron_pickaxe", damage = 100, repairCost = 39),
                stack("iron_ingot"),
                isRepairMaterial = Anvils::isRepairMaterial,
            )

        assertNull(result.output, "the anvil refuses")
        assertEquals(40, result.cost)
    }

    /** But a rename alone is capped rather than refused. */
    @Test
    fun aRenameIsCappedAtThirtyNine() {
        val result = Anvils.combine(stack("iron_pickaxe", repairCost = 50), null, newName = "Expensive")

        assertNotNull(result.output)
        assertEquals(39, result.cost)
    }
}
