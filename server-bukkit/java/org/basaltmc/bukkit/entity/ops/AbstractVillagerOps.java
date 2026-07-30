package org.basaltmc.bukkit.entity.ops;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.entity.EntityItems;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.entity.AbstractVillager;
import org.bukkit.entity.HumanEntity;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.MerchantRecipe;

public interface AbstractVillagerOps extends AbstractVillager, Cored {
    private static CompoundTag recipeTag(MerchantRecipe recipe) {
        CompoundTag tag = new CompoundTag();
        List<ItemStack> ingredients = recipe.getIngredients();
        if (ingredients.isEmpty()) {
            throw new IllegalArgumentException("A trade needs at least one ingredient");
        }
        tag.put("buy", EntityItems.tag(ingredients.get(0)));
        if (ingredients.size() > 1 && EntityItems.tag(ingredients.get(1)) != null) {
            tag.put("buyB", EntityItems.tag(ingredients.get(1)));
        }
        tag.put("sell", EntityItems.tag(recipe.getResult()));
        tag.putInt("uses", recipe.getUses());
        tag.putInt("maxUses", recipe.getMaxUses());
        tag.putBoolean("rewardExp", recipe.hasExperienceReward());
        tag.putInt("specialPrice", recipe.getSpecialPrice());
        tag.putInt("demand", recipe.getDemand());
        tag.putFloat("priceMultiplier", recipe.getPriceMultiplier());
        tag.putInt("xp", recipe.getVillagerExperience());
        return tag;
    }

    private static ItemStack stack(CompoundTag tag) {
        byte[] bytes = NbtCodec.toBytes(tag);
        return EntityItems.fromNbt(bytes);
    }

    default List<MerchantRecipe> getRecipes() {
        core().call("villager_add_trades", 0L);
        List<MerchantRecipe> out = new ArrayList<>();
        byte[] stored = core().nbt("Offers");
        CompoundTag offers = stored == null ? null : NbtCodec.fromBytes(stored);
        if (offers == null) {
            return out;
        }
        ListTag recipes = offers.getListOrEmpty("Recipes");
        for (int at = 0; at < recipes.size(); at++) {
            CompoundTag tag = recipes.getCompoundOrEmpty(at);
            ItemStack result = stack(tag.getCompoundOrEmpty("sell"));
            ItemStack first = stack(tag.getCompoundOrEmpty("buy"));
            if (result == null || first == null) {
                continue;
            }
            MerchantRecipe recipe = new MerchantRecipe(result, tag.getIntOr("uses", 0), tag.getIntOr("maxUses", 4),
                tag.getBooleanOr("rewardExp", true), tag.getIntOr("xp", 0), tag.getFloatOr("priceMultiplier", 0f),
                tag.getIntOr("demand", 0), tag.getIntOr("specialPrice", 0));
            recipe.addIngredient(first);
            ItemStack second = tag.contains("buyB") ? stack(tag.getCompoundOrEmpty("buyB")) : null;
            if (second != null) {
                recipe.addIngredient(second);
            }
            out.add(recipe);
        }
        return out;
    }

    default void setRecipes(List<MerchantRecipe> recipes) {
        ListTag list = new ListTag();
        for (MerchantRecipe recipe : recipes) {
            list.add(recipeTag(recipe));
        }
        CompoundTag offers = new CompoundTag();
        offers.put("Recipes", list);
        CompoundTag patch = new CompoundTag();
        patch.put("Offers", offers);
        core().mergeNbt(NbtCodec.toBytes(patch));
    }

    default int getRecipeCount() {
        return getRecipes().size();
    }

    default MerchantRecipe getRecipe(int index) throws IndexOutOfBoundsException {
        return getRecipes().get(index);
    }

    default void setRecipe(int index, MerchantRecipe recipe) throws IndexOutOfBoundsException {
        List<MerchantRecipe> recipes = getRecipes();
        recipes.set(index, recipe);
        setRecipes(recipes);
    }

    default boolean isTrading() {
        return getTrader() != null;
    }

    default HumanEntity getTrader() {
        int trader = core().integer("trading_player");
        if (trader < 0) {
            return null;
        }
        for (org.bukkit.entity.Player player : org.bukkit.Bukkit.getOnlinePlayers()) {
            if (player.getEntityId() == trader) {
                return player;
            }
        }
        return null;
    }
}
