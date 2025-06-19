package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.item.ItemWear
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtString

/**
 * What an anvil makes of two items, and what it charges.
 *
 * parity: `AnvilMenu.createResult`, in its order: the repair cost of both items is the tax, the work
 * itself is the price, renaming is one level on top, and the whole thing is refused at forty. The
 * prior-work cost doubles and adds one every time, which is what makes an item "too expensive".
 *
 * Merging enchantments needs each enchantment's own `anvil_cost` and its exclusive set, both of
 * which come from the jar's enchantment files.
 */
public object Anvils {
    /** parity: `AnvilMenu.MAX_COST`, the forty levels survival will not pay. */
    public const val TOO_EXPENSIVE: Int = 40

    /** What an anvil would produce, and what it asks for it. */
    public class Result(
        public val output: StoredItemStack?,
        public val cost: Int,
        public val materialCount: Int = 0,
    )

    /**
     * parity: `AnvilMenu.createResult`, minus the enchantment merge, which is handed in as
     * [mergeEnchantments] so this stays a pure function of the two stacks.
     */
    public fun combine(
        input: StoredItemStack?,
        addition: StoredItemStack?,
        newName: String? = null,
        isRepairMaterial: (StoredItemStack, StoredItemStack) -> Boolean = { _, _ -> false },
        mergeEnchantments: (StoredItemStack, StoredItemStack) -> Pair<NbtCompound?, Int> = { _, _ -> null to 0 },
    ): Result {
        if (input == null) return Result(null, 0)
        var price = 0
        val tax = repairCost(input) + (addition?.let { repairCost(it) } ?: 0)
        var result = input.copy()
        var materials = 0

        if (addition != null) {
            val damageable = ItemWear.isDamageable(input)
            if (damageable && isRepairMaterial(input, addition)) {
                // parity: a quarter of the maximum per material, and one level for each one used.
                var damage = ItemWear.damage(result)
                val max = ItemWear.maxDamage(result)
                if (damage <= 0) return Result(null, 0)
                var used = 0
                while (damage > 0 && used < addition.count) {
                    damage = (damage - minOf(damage, max / 4)).coerceAtLeast(0)
                    price++
                    used++
                }
                materials = used
                result = withDamage(result, damage)
            } else if (input.id == addition.id && damageable) {
                // parity: the two durabilities added, plus twelve per cent of the maximum.
                val max = ItemWear.maxDamage(input)
                val left = max - ItemWear.damage(input)
                val extra = max - ItemWear.damage(addition) + max * 12 / 100
                val damage = (max - (left + extra)).coerceAtLeast(0)
                if (damage < ItemWear.damage(input)) {
                    result = withDamage(result, damage)
                    price += 2
                }
                val (merged, enchantmentCost) = mergeEnchantments(result, addition)
                if (merged != null) result = result.copy(components = merged)
                price += enchantmentCost
            } else {
                val (merged, enchantmentCost) = mergeEnchantments(result, addition)
                if (merged == null && enchantmentCost == 0) return Result(null, 0)
                if (merged != null) result = result.copy(components = merged)
                price += enchantmentCost
            }
        }

        // parity: the naming, which is one level whether it adds a name or takes one away.
        var namingCost = 0
        if (newName != null && newName.isNotEmpty()) {
            if (newName != nameOf(input)) {
                namingCost = 1
                price += namingCost
                result = withName(result, newName)
            }
        } else if (nameOf(input) != null) {
            namingCost = 1
            price += namingCost
            result = withName(result, null)
        }

        if (price <= 0) return Result(null, 0)
        var cost = (tax.toLong() + price).coerceIn(0L, Int.MAX_VALUE.toLong()).toInt()
        // parity: renaming alone is never refused; it is capped one under the limit instead.
        if (namingCost == price && namingCost > 0 && cost >= TOO_EXPENSIVE) cost = TOO_EXPENSIVE - 1
        if (cost >= TOO_EXPENSIVE) return Result(null, cost)

        var priorWork = maxOf(repairCost(result), addition?.let { repairCost(it) } ?: 0)
        if (namingCost != price || namingCost == 0) priorWork = increasedRepairCost(priorWork)
        return Result(withRepairCost(result, priorWork), cost, materials)
    }

    /** parity: `AnvilMenu.calculateIncreasedRepairCost`: it doubles and adds one, every time. */
    public fun increasedRepairCost(cost: Int): Int = (cost * 2 + 1).coerceAtMost(Int.MAX_VALUE)

    public fun repairCost(stack: StoredItemStack): Int =
        (stack.components[ItemComponentCodecs.REPAIR_COST] as? NbtInt)?.value ?: 0

    private fun withRepairCost(
        stack: StoredItemStack,
        cost: Int,
    ): StoredItemStack {
        val components = NbtCompound(stack.components.associate { it.key to it.value })
        components[ItemComponentCodecs.REPAIR_COST] = NbtInt(cost)
        return stack.copy(components = components)
    }

    private fun withDamage(
        stack: StoredItemStack,
        damage: Int,
    ): StoredItemStack {
        val components = NbtCompound(stack.components.associate { it.key to it.value })
        components[ItemComponentCodecs.DAMAGE] = NbtInt(damage)
        return stack.copy(components = components)
    }

    private fun nameOf(stack: StoredItemStack): String? =
        (stack.components[ItemComponentCodecs.CUSTOM_NAME] as? NbtString)?.value

    private fun withName(
        stack: StoredItemStack,
        name: String?,
    ): StoredItemStack {
        val components = NbtCompound(stack.components.associate { it.key to it.value })
        if (name == null) {
            components.remove(ItemComponentCodecs.CUSTOM_NAME)
        } else {
            components[ItemComponentCodecs.CUSTOM_NAME] = NbtString(name)
        }
        return stack.copy(components = components)
    }

    /** parity: `ItemStack.isValidRepairItem` for the vanilla tool and armour materials. */
    public fun isRepairMaterial(
        input: StoredItemStack,
        addition: StoredItemStack,
    ): Boolean {
        val plain = input.id.removePrefix("minecraft:")
        val material = REPAIR_MATERIALS.entries.firstOrNull { (prefix, _) -> plain.startsWith(prefix) } ?: return false
        return addition.id == "minecraft:${material.value}"
    }

    /**
     * parity: the `repair_items` of each tool material in the jar's item data. The five vanilla tool
     * and armour families, by the prefix their items share.
     */
    private val REPAIR_MATERIALS: Map<String, String> =
        mapOf(
            "wooden_" to "oak_planks",
            "stone_" to "cobblestone",
            "iron_" to "iron_ingot",
            "golden_" to "gold_ingot",
            "diamond_" to "diamond",
            "netherite_" to "netherite_ingot",
            "leather_" to "leather",
            "chainmail_" to "iron_ingot",
        )
}
