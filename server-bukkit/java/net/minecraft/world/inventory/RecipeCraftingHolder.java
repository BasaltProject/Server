// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;
import java.util.List;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.RecipeHolder;
public interface RecipeCraftingHolder {
    void setRecipeUsed(RecipeHolder<?> recipeUsed);
    RecipeHolder<?> getRecipeUsed();
    default void awardUsedRecipes(Player player, List<ItemStack> itemStacks) {
        RecipeHolder<?> used = getRecipeUsed();
        if (used == null) {
            return;
        }
        player.triggerRecipeCrafted(used, itemStacks);
        if (!used.value().isSpecial()) {
            player.awardRecipes(java.util.Collections.singleton(used));
            setRecipeUsed((RecipeHolder<?>) null);
        }
    }
    default boolean setRecipeUsed(ServerPlayer player, RecipeHolder<?> recipe) {
        boolean blocked = !recipe.value().isSpecial() && player.level().getGameRules().get(net.minecraft.world.level.gamerules.GameRules.LIMITED_CRAFTING)
            && !player.getRecipeBook().contains(recipe.id());
        if (blocked) {
            return false;
        }
        setRecipeUsed(recipe);
        return true;
    }
}
