package org.basaltmc.bukkit.recipe;

import org.bukkit.NamespacedKey;
import org.bukkit.inventory.ComplexRecipe;
import org.bukkit.inventory.ItemStack;

/** One of the recipes the game computes (dyeing, firework crafting, banner copying...), which can be read, not made. */
public final class SpecialRecipe implements ComplexRecipe {
    private final NamespacedKey key;
    private final ItemStack result;

    SpecialRecipe(NamespacedKey key, ItemStack result) {
        this.key = key;
        this.result = result;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public ItemStack getResult() {
        return result.clone();
    }
}
