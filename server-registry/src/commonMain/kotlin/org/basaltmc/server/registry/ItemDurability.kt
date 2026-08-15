package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** One damageable item: how much wear it survives, and what a block or an attack costs it. */
public data class ItemDurabilityEntry(
    public val maxDamage: Int,
    public val damagePerBlock: Int,
    public val damagePerAttack: Int,
)

/**
 * Which items wear out, and how fast. From [ALL_ITEM_DURABILITY], the jar's own component reports.
 *
 * parity: an item with no entry has no `minecraft:max_damage` component in vanilla either, and
 * vanilla's `ItemStack.hurtAndBreak` returns immediately for one, so it never wears rather than
 * wearing slowly.
 */
public object ItemDurability {
    private val byName: Map<Identifier, ItemDurabilityEntry> =
        ALL_ITEM_DURABILITY.associate { (path, entry) -> Identifier.vanilla(path) to entry }

    public operator fun get(item: Identifier): ItemDurabilityEntry? = byName[item]

    public fun maxDamageOf(item: Identifier): Int = byName[item]?.maxDamage ?: 0

    public fun isDamageable(item: Identifier): Boolean = item in byName
}
