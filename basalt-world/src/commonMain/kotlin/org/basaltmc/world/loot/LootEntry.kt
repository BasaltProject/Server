package org.basaltmc.world.loot

import org.basaltmc.random.BasaltRandom
import org.basaltmc.registry.Identifier
import kotlin.math.floor
import kotlin.math.max

/**
 * A single `minecraft:item` entry in a pool. Mirrors vanilla's `LootItem`/`LootPoolSingletonContainer`;
 * the composite entry types (`alternatives`, `group`, `sequence`, `tag`, nested `loot_table`) are a
 * documented gap, since flat item entries alone already cover the overwhelming majority of real
 * vanilla loot tables.
 */
public data class LootEntry(
    val item: Identifier,
    val weight: Int = 1,
    val quality: Int = 0,
    val conditions: List<LootCondition> = emptyList(),
    val functions: List<LootFunction> = emptyList(),
) {
    /** `max(floor(weight + quality * luck), 0)`: verified against the real jar's decompiled `EntryBase`. */
    internal fun weightFor(luck: Float): Int = max(floor(weight + quality * luck).toInt(), 0)

    internal fun canRun(random: BasaltRandom): Boolean = conditions.all { it.test(random) }

    internal fun createItemStack(random: BasaltRandom): ItemStack {
        var stack = ItemStack(item, 1)
        for (function in functions) stack = function.apply(stack, random)
        return stack
    }
}
