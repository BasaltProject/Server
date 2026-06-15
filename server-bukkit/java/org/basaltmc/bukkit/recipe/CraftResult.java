package org.basaltmc.bukkit.recipe;

import java.util.List;
import org.bukkit.inventory.ItemCraftResult;
import org.bukkit.inventory.ItemStack;

public final class CraftResult implements ItemCraftResult {
    private final ItemStack result;
    private final ItemStack[] matrix;

    public CraftResult(ItemStack result, ItemStack[] matrix) {
        this.result = result;
        this.matrix = matrix;
    }

    @Override
    public ItemStack getResult() {
        return result;
    }

    @Override
    public ItemStack[] getResultingMatrix() {
        return matrix.clone();
    }

    @Override
    public List<ItemStack> getOverflowItems() {
        return List.of();
    }
}
