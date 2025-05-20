package org.basaltmc.world.crafting

import org.basaltmc.registry.Identifier
import org.basaltmc.world.loot.ItemStack

/**
 * Accepts any item in [items], a set already flattened from a tag reference if the recipe JSON used
 * one (`#namespace:path`). Mirrors vanilla's `Ingredient`, whose `HolderSet<Item>` is resolved the
 * same way at decode time rather than kept as a lazy tag lookup.
 */
public data class Ingredient(
    val items: Set<Identifier>,
) {
    public fun test(stack: ItemStack?): Boolean = stack != null && stack.item in items

    public companion object {
        /** Mirrors `Ingredient.testOptionalIngredient`: an empty slot matches only an absent ingredient. */
        public fun testOptional(
            ingredient: Ingredient?,
            stack: ItemStack?,
        ): Boolean = if (ingredient != null) ingredient.test(stack) else stack == null
    }
}
