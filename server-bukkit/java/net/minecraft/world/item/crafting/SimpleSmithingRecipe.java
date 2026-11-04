// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
public abstract class SimpleSmithingRecipe implements SmithingRecipe {
    protected final Recipe.CommonInfo commonInfo;
    protected SimpleSmithingRecipe(Recipe.CommonInfo commonInfo) {
        this.commonInfo = commonInfo;
    }
    public abstract RecipeSerializer<? extends SimpleSmithingRecipe> getSerializer();
    public PlacementInfo placementInfo() {
        return null;
    }
    protected abstract PlacementInfo createPlacementInfo();
    public String group() {
        return "";
    }
    public final boolean showNotification() {
        return commonInfo.showNotification();
    }
}
