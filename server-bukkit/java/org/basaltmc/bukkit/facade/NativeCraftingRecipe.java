package org.basaltmc.bukkit.facade;

import java.util.List;
import net.minecraft.core.NonNullList;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.CraftingBookCategory;
import net.minecraft.world.item.crafting.CraftingInput;
import net.minecraft.world.item.crafting.CraftingRecipe;
import net.minecraft.world.item.crafting.PlacementInfo;
import net.minecraft.world.item.crafting.RecipeSerializer;
import net.minecraft.world.level.Level;
import org.basaltmc.bukkit.recipe.Recipes;
import org.bukkit.craftbukkit.inventory.CraftItemStack;

/** A crafting recipe whose matching and result come from the server's recipe table. */
public final class NativeCraftingRecipe implements CraftingRecipe {
    private final Identifier id;

    public NativeCraftingRecipe(Identifier id) {
        this.id = id;
    }

    public Identifier id() {
        return id;
    }

    public static Recipes.Crafted craftPublic(CraftingInput input) {
        return craft(input);
    }

    static Recipes.Crafted craft(CraftingInput input) {
        int side = input.width() <= 2 && input.height() <= 2 ? 2 : 3;
        org.bukkit.inventory.ItemStack[] matrix = new org.bukkit.inventory.ItemStack[side * side];
        for (int y = 0; y < input.height() && y < side; y++) {
            for (int x = 0; x < input.width() && x < side; x++) {
                matrix[x + y * side] = CraftItemStack.asBukkitMirror(input.getItem(x, y));
            }
        }
        return Recipes.craft(matrix);
    }

    private boolean produces(Recipes.Crafted crafted) {
        return crafted != null && (id == null || crafted.recipe() == null || crafted.recipe().toString().equals(id.toString()));
    }

    @Override
    public boolean matches(CraftingInput input, Level level) {
        return !input.isEmpty() && produces(craft(input));
    }

    @Override
    public ItemStack assemble(CraftingInput input) {
        Recipes.Crafted crafted = craft(input);
        return produces(crafted) ? CraftItemStack.asNMSCopy(crafted.result()) : ItemStack.EMPTY;
    }

    @Override
    public NonNullList<ItemStack> getRemainingItems(CraftingInput input) {
        Recipes.Crafted crafted = craft(input);
        NonNullList<ItemStack> left = NonNullList.withSize(input.size(), ItemStack.EMPTY);
        if (produces(crafted)) {
            int side = input.width() <= 2 && input.height() <= 2 ? 2 : 3;
            for (int y = 0; y < input.height() && y < side; y++) {
                for (int x = 0; x < input.width() && x < side; x++) {
                    org.bukkit.inventory.ItemStack rest = crafted.remaining()[x + y * side];
                    if (rest != null) {
                        left.set(x + y * input.width(), CraftItemStack.asNMSCopy(rest));
                    }
                }
            }
        }
        return left;
    }

    @Override
    public boolean showNotification() {
        return true;
    }

    @Override
    public String group() {
        return "";
    }

    @Override
    public RecipeSerializer<? extends CraftingRecipe> getSerializer() {
        return null;
    }

    @Override
    public CraftingBookCategory category() {
        return null;
    }

    @Override
    public PlacementInfo placementInfo() {
        return null;
    }

    @Override
    public boolean isSpecial() {
        return false;
    }

    @Override
    public String toString() {
        return "NativeCraftingRecipe[" + id + "]";
    }

    public static List<Identifier> allIds() {
        String[] ids = org.basaltmc.bukkit.InventoryNatives.recipeIds();
        return ids == null ? List.of() : java.util.Arrays.stream(ids).map(Identifier::parse).toList();
    }
}
