// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
public abstract class CustomRecipe implements CraftingRecipe {
    public boolean isSpecial() {
        return true;
    }
    public boolean showNotification() {
        return true;
    }
    public String group() {
        return "";
    }
    public CraftingBookCategory category() {
        return CraftingBookCategory.MISC;
    }
    public PlacementInfo placementInfo() {
        return PlacementInfo.NOT_PLACEABLE;
    }
    public abstract RecipeSerializer<? extends CustomRecipe> getSerializer();
}
