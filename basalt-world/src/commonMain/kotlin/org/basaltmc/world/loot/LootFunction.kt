package org.basaltmc.world.loot

import org.basaltmc.random.BasaltRandom

/**
 * A post-processing step applied to a generated [ItemStack]. Mirrors vanilla's `LootItemFunction`;
 * only `minecraft:set_count` is implemented so far (extremely common: most block drops use it to
 * roll a random count). The dozens of others (`enchant_randomly`, `smelt`, `explosion_decay`,
 * `looting_enchant`...) are a documented gap.
 */
public sealed interface LootFunction {
    public fun apply(
        stack: ItemStack,
        random: BasaltRandom,
    ): ItemStack

    public data class SetCount(
        val count: NumberProvider,
        val add: Boolean = false,
    ) : LootFunction {
        override fun apply(
            stack: ItemStack,
            random: BasaltRandom,
        ): ItemStack {
            val base = if (add) stack.count else 0
            stack.count = base + count.getInt(random)
            return stack
        }
    }
}
