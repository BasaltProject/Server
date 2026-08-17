package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString

/**
 * What the enchantments on a player's armour are worth against one particular hit.
 *
 * parity: `EnchantmentHelper.getDamageProtection`, which sums each piece's `damage_protection`
 * effects. The requirement on each one is a damage source predicate, and every vanilla protection
 * writes it the same way: a list of damage type tags the source must or must not be in, which is
 * why [RealDamageTypeTags] is enough to answer them all rather than a general predicate engine.
 */
public object DamageProtection {
    public fun of(
        worn: List<StoredItemStack?>,
        damageType: Identifier?,
        tags: RealDamageTypeTags,
        definitionOf: (Identifier) -> NbtCompound?,
    ): Float {
        var protection = 0f
        for (stack in worn) {
            if (stack == null) continue
            for ((id, level) in PlayerInventory.enchantmentLevels(stack)) {
                val definition = definitionOf(id) ?: continue
                val effects = (definition["effects"] as? NbtCompound) ?: continue
                val entries = (effects["minecraft:damage_protection"] as? NbtList) ?: continue
                for (entry in entries.filterIsInstance<NbtCompound>()) {
                    if (!matches(entry["requirements"] as? NbtCompound, damageType, tags)) continue
                    protection += amountOf(entry["effect"] as? NbtCompound, level)
                }
            }
        }
        return protection
    }

    /** parity: `LevelBasedValue.Linear`, which is the only shape a protection uses. */
    private fun amountOf(
        effect: NbtCompound?,
        level: Int,
    ): Float {
        val value = effect?.get("value") as? NbtCompound ?: return 0f
        val base = value.getDouble("base").toFloat()
        return base + value.getDouble("per_level_above_first").toFloat() * (level - 1)
    }

    private fun matches(
        requirements: NbtCompound?,
        damageType: Identifier?,
        tags: RealDamageTypeTags,
    ): Boolean {
        if (requirements == null) return true
        return when (requirements.getString("condition")) {
            "minecraft:all_of" ->
                (requirements["terms"] as? NbtList)
                    ?.filterIsInstance<NbtCompound>()
                    ?.all { matches(it, damageType, tags) } ?: true
            "minecraft:any_of" ->
                (requirements["terms"] as? NbtList)
                    ?.filterIsInstance<NbtCompound>()
                    ?.any { matches(it, damageType, tags) } ?: true
            "minecraft:damage_source_properties" -> matchesSource(requirements, damageType, tags)
            // A condition this does not know is treated as unmet: counting protection that vanilla
            // would not is worse than missing some, since it would make armour stronger than real.
            else -> false
        }
    }

    private fun matchesSource(
        condition: NbtCompound,
        damageType: Identifier?,
        tags: RealDamageTypeTags,
    ): Boolean {
        val predicate = condition["predicate"] as? NbtCompound ?: return true
        val tests = predicate["tags"] as? NbtList ?: return true
        if (damageType == null) return false
        for (test in tests.filterIsInstance<NbtCompound>()) {
            val tag = (test["id"] as? NbtString)?.value?.let(Identifier::parse) ?: continue
            val expected = test.getBoolean("expected")
            if (tags.isIn(tag, damageType) != expected) return false
        }
        return true
    }
}
