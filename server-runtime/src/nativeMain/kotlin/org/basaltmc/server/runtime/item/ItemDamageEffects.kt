package org.basaltmc.server.runtime.item

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtTag

/**
 * How much wear an enchantment takes off a use. parity: `EnchantmentHelper.processDurabilityChange`
 * over each enchantment's own `minecraft:item_damage` effect.
 *
 * Read from the operator's own enchantment definitions rather than hard-coded, so unbreaking is not
 * a special case in this server and a datapack that writes the same effect gets the same treatment.
 * Only `remove_binomial` is implemented, which is the only value effect vanilla's own durability
 * enchantments use; any other type leaves the amount alone rather than guessing at it.
 */
public class ItemDamageEffects(
    /** An enchantment's definition as the jar wrote it, or null where the registry never loaded. */
    private val definitionOf: (Identifier) -> NbtCompound?,
    /** Whether an item is in an item tag, for the effects that only apply to some of them. */
    private val itemInTag: (String, Identifier) -> Boolean = { _, _ -> false },
) {
    public fun process(
        stack: StoredItemStack,
        amount: Int,
        random: BasaltRandom,
    ): Int {
        val levels = PlayerInventory.enchantmentLevels(stack)
        if (levels.isEmpty()) return amount
        var value = amount.toFloat()
        for ((enchantment, level) in levels) {
            val definition = definitionOf(enchantment) ?: continue
            for (effect in conditionalEffects(definition)) {
                if (!matches(effect, stack)) continue
                value = apply(effect["effect"] as? NbtCompound ?: continue, level, value, random)
            }
        }
        return value.toInt()
    }

    private fun conditionalEffects(definition: NbtCompound): List<NbtCompound> {
        val effects = definition["effects"] as? NbtCompound ?: return emptyList()
        val list = effects[ITEM_DAMAGE] as? NbtList ?: return emptyList()
        return (0 until list.size).mapNotNull { list[it] as? NbtCompound }
    }

    /** parity: `ConditionalEffect`, of which only the `match_tool` item predicate is modelled. */
    private fun matches(
        effect: NbtCompound,
        stack: StoredItemStack,
    ): Boolean {
        val requirements = effect["requirements"] as? NbtCompound ?: return true
        val predicate = requirements["predicate"] as? NbtCompound ?: return true
        val items = (predicate["items"] as? NbtString)?.value ?: return true
        val item = Identifier.parse(stack.id)
        if (!items.startsWith("#")) return Identifier.parse(items) == item
        return itemInTag(items.removePrefix("#"), item)
    }

    private fun apply(
        effect: NbtCompound,
        level: Int,
        value: Float,
        random: BasaltRandom,
    ): Float {
        if ((effect["type"] as? NbtString)?.value != REMOVE_BINOMIAL) return value
        val chance = levelBasedValue(effect["chance"], level) ?: return value
        // parity: `RemoveBinomial.process`, the per-point roll. Its normal approximation above 128
        // points is left out because nothing here ever asks for that much wear at once.
        var dropped = 0
        var rolls = 0
        while (rolls < value.toInt()) {
            if (random.nextFloat() < chance) dropped++
            rolls++
        }
        return value - dropped
    }

    /** parity: `LevelBasedValue`, the shapes durability effects actually use. */
    private fun levelBasedValue(
        tag: NbtTag?,
        level: Int,
    ): Float? {
        numberOf(tag)?.let { return it }
        val compound = tag as? NbtCompound ?: return null
        return when ((compound["type"] as? NbtString)?.value) {
            "minecraft:constant" -> numberOf(compound["value"])
            "minecraft:linear" -> {
                val base = numberOf(compound["base"]) ?: return null
                val perLevel = numberOf(compound["per_level_above_first"]) ?: 0f
                base + perLevel * (level - 1)
            }
            "minecraft:fraction" -> {
                val denominator = levelBasedValue(compound["denominator"], level) ?: return null
                if (denominator == 0f) return 0f
                val numerator = levelBasedValue(compound["numerator"], level) ?: return null
                numerator / denominator
            }
            else -> null
        }
    }

    private fun numberOf(tag: NbtTag?): Float? =
        when (tag) {
            is NbtFloat -> tag.value
            is NbtDouble -> tag.value.toFloat()
            is NbtInt -> tag.value.toFloat()
            else -> null
        }

    private companion object {
        const val ITEM_DAMAGE = "minecraft:item_damage"
        const val REMOVE_BINOMIAL = "minecraft:remove_binomial"
    }
}
