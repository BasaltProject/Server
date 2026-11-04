// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
public abstract class NormalCraftingRecipe implements CraftingRecipe {
    protected final Recipe.CommonInfo commonInfo;
    protected final CraftingRecipe.CraftingBookInfo bookInfo;
    private PlacementInfo placementInfo;
    protected NormalCraftingRecipe(Recipe.CommonInfo commonInfo, CraftingRecipe.CraftingBookInfo bookInfo) {
        this.commonInfo = commonInfo;
        this.bookInfo = bookInfo;
    }
    public abstract RecipeSerializer<? extends NormalCraftingRecipe> getSerializer();
    public final String group() {
        return bookInfo.group();
    }
    public final CraftingBookCategory category() {
        return bookInfo.category();
    }
    public final boolean showNotification() {
        return commonInfo.showNotification();
    }
    protected abstract PlacementInfo createPlacementInfo();
    public final PlacementInfo placementInfo() {
        if (placementInfo == null) {
            placementInfo = createPlacementInfo();
        }
        return placementInfo;
    }
}
