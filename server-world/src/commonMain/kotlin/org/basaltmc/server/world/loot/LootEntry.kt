package org.basaltmc.server.world.loot

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.wire.common.Identifier
import kotlin.math.floor
import kotlin.math.max

/**
 * A single entry in a pool: either a `minecraft:item` leaf ([item] set, [alternatives] empty) or a
 * `minecraft:alternatives` composite ([item] null, [alternatives] the children tried in order).
 * Mirrors vanilla's `LootItem`/`LootPoolSingletonContainer` and `AlternativesEntry`; the other
 * composite types (`group`, `sequence`, `tag`, nested `loot_table`) are a documented gap, since
 * these two alone already cover the overwhelming majority of real vanilla block loot tables (a
 * silk-touch-or-not choice, almost always).
 */
public data class LootEntry(
    val item: Identifier? = null,
    val weight: Int = 1,
    val quality: Int = 0,
    val conditions: List<LootCondition> = emptyList(),
    val functions: List<LootFunction> = emptyList(),
    val alternatives: List<LootEntry> = emptyList(),
) {
    /** `max(floor(weight + quality * luck), 0)`: verified against the real jar's decompiled `EntryBase`. */
    internal fun weightFor(luck: Float): Int = max(floor(weight + quality * luck).toInt(), 0)

    internal fun canRun(random: BasaltRandom): Boolean = conditions.all { it.test(random) }

    internal fun createItemStack(random: BasaltRandom): ItemStack {
        var stack = ItemStack(requireNotNull(item) { "an alternatives entry has no item of its own" }, 1)
        for (function in functions) stack = function.apply(stack, random)
        return stack
    }
}
