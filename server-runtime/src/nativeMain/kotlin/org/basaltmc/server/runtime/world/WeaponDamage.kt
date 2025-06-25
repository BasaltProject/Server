package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList

/**
 * What the enchantments on a weapon add to a hit.
 *
 * parity: `EnchantmentHelper.modifyDamage` over each enchantment's `minecraft:damage` effects.
 * The ones that ask something of what was hit (smite, bane of arthropods) carry a requirement on
 * the target's entity type, and they are left out until entity type tags are loaded: counting them
 * against everything would make a sword hit harder than vanilla's.
 */
public object WeaponDamage {
    public fun of(
        stack: StoredItemStack?,
        base: Float,
        definitionOf: (Identifier) -> NbtCompound?,
    ): Float {
        if (stack == null) return base
        var damage = base
        for ((id, level) in PlayerInventory.enchantmentLevels(stack)) {
            val definition = definitionOf(id) ?: continue
            val effects = (definition["effects"] as? NbtCompound) ?: continue
            val entries = (effects["minecraft:damage"] as? NbtList) ?: continue
            for (entry in entries.filterIsInstance<NbtCompound>()) {
                if (entry["requirements"] != null) continue
                damage = apply(entry["effect"] as? NbtCompound, damage, level)
            }
        }
        return damage
    }

    private fun apply(
        effect: NbtCompound?,
        damage: Float,
        level: Int,
    ): Float {
        if (effect == null) return damage
        val amount = amountOf(effect["value"] as? NbtCompound, level)
        return when (effect.getString("type")) {
            "minecraft:add" -> damage + amount
            "minecraft:multiply" -> damage * amount
            else -> damage
        }
    }

    /** parity: `LevelBasedValue.Linear`, the shape every damage effect uses. */
    private fun amountOf(
        value: NbtCompound?,
        level: Int,
    ): Float {
        if (value == null) return 0f
        val base = value.getDouble("base").toFloat()
        return base + value.getDouble("per_level_above_first").toFloat() * (level - 1)
    }
}
