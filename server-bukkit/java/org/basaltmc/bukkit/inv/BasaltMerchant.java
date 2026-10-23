package org.basaltmc.bukkit.inv;

import java.util.ArrayList;
import java.util.List;
import net.kyori.adventure.text.Component;
import org.bukkit.entity.HumanEntity;
import org.bukkit.inventory.Merchant;
import org.bukkit.inventory.MerchantRecipe;

/** A trader that is not an entity: a title and a list of trades a plugin made. */
public final class BasaltMerchant implements Merchant {
    private final Component title;
    private final List<MerchantRecipe> recipes = new ArrayList<>();

    public BasaltMerchant(Component title) {
        this.title = title;
    }

    public Component title() {
        return title;
    }

    @Override
    public List<MerchantRecipe> getRecipes() {
        return List.copyOf(recipes);
    }

    @Override
    public void setRecipes(List<MerchantRecipe> list) {
        recipes.clear();
        recipes.addAll(list);
    }

    @Override
    public MerchantRecipe getRecipe(int index) {
        return recipes.get(index);
    }

    @Override
    public void setRecipe(int index, MerchantRecipe recipe) {
        recipes.set(index, recipe);
    }

    @Override
    public int getRecipeCount() {
        return recipes.size();
    }

    @Override
    public boolean isTrading() {
        return false;
    }

    @Override
    public HumanEntity getTrader() {
        return null;
    }
}
