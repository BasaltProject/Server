package org.basaltmc.server.world.loot

import org.basaltmc.server.random.BasaltRandom

/**
 * A post-processing step applied to a generated [ItemStack]. Mirrors vanilla's `LootItemFunction`:
 * `minecraft:set_count` (which most drops use to roll a count) and
 * `minecraft:enchanted_count_increase` (the looting bonus, which every mob's own table carries).
 * The rest (`enchant_randomly`, `smelt`, `explosion_decay`...) are a documented gap.
 */
public sealed interface LootFunction {
    public fun apply(
        stack: ItemStack,
        random: BasaltRandom,
    ): ItemStack

    /**
     * parity: `EnchantedCountIncreaseFunction`: the killer's enchantment level times the roll, added
     * to the count. Nothing here carries an enchantment into a loot roll yet, so the level is zero
     * and the function adds nothing, which is exactly what an unenchanted kill does in vanilla.
     */
    public data class EnchantedCountIncrease(
        val count: NumberProvider,
        val limit: Int? = null,
    ) : LootFunction {
        override fun apply(
            stack: ItemStack,
            random: BasaltRandom,
        ): ItemStack {
            val level = 0
            if (level == 0) return stack
            stack.count += level * count.getInt(random)
            limit?.let { stack.count = minOf(stack.count, it) }
            return stack
        }
    }

    /**
     * Something copied off the block entity onto the stack: contents, a name, part of the state.
     *
     * parity: `CopyComponentsFunction` and its three cousins. The copying itself belongs to whoever
     * knows the block, so this only says the table is understood: a table refused for one function
     * it did not know dropped nothing at all, which is how a shulker box came to vanish.
     */
    public data object CopyFromBlock : LootFunction {
        override fun apply(
            stack: ItemStack,
            random: BasaltRandom,
        ): ItemStack = stack
    }

    /**
     * parity: `SmeltItemFunction`, which is what a burning cow's cooked beef comes from. Nothing
     * here dies on fire yet, so the drop is left raw, which is what an ordinary kill gives.
     */
    public data object FurnaceSmelt : LootFunction {
        override fun apply(
            stack: ItemStack,
            random: BasaltRandom,
        ): ItemStack = stack
    }

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
