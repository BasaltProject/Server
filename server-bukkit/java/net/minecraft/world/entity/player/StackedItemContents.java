// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.Recipe;
public class StackedItemContents {
    private final StackedContents<Holder<Item>> raw = new StackedContents<>();
    private static List<net.minecraft.world.item.crafting.Ingredient> ingredientsOf(Recipe<?> recipe) {
        net.minecraft.world.item.crafting.PlacementInfo info = recipe.placementInfo();
        return info == null || info.isImpossibleToPlace() ? null : info.ingredients();
    }
    public void accountSimpleStack(ItemStack itemStack) {
        if (Inventory.isUsableForCrafting(itemStack)) {
            accountStack(itemStack);
        }
    }
    public void accountStack(ItemStack itemStack) {
        accountStack(itemStack, itemStack.getMaxStackSize());
    }
    public void accountStack(ItemStack itemStack, int maxCount) {
        if (!itemStack.isEmpty()) {
            raw.account(itemStack.typeHolder(), Math.min(maxCount, itemStack.getCount()));
        }
    }
    public boolean canCraft(Recipe<?> recipe, StackedContents. Output<Holder<Item>> output) {
        return canCraft(recipe, 1, output);
    }
    public boolean canCraft(Recipe<?> recipe, int amount, StackedContents. Output<Holder<Item>> output) {
        List<net.minecraft.world.item.crafting.Ingredient> ingredients = ingredientsOf(recipe);
        return ingredients != null && canCraft(ingredients, amount, output);
    }
    public boolean canCraft(List<? extends StackedContents.IngredientInfo<Holder<Item>>> contents, StackedContents. Output<Holder<Item>> output) {
        return raw.tryPick(contents, 1, output);
    }
    public int getBiggestCraftableStack(Recipe<?> recipe, StackedContents. Output<Holder<Item>> output) {
        return getBiggestCraftableStack(recipe, Integer.MAX_VALUE, output);
    }
    public int getBiggestCraftableStack(Recipe<?> recipe, int maxSize, StackedContents. Output<Holder<Item>> output) {
        List<net.minecraft.world.item.crafting.Ingredient> ingredients = ingredientsOf(recipe);
        return ingredients == null ? 0 : raw.tryPickAll(ingredients, maxSize, output);
    }
    public void clear() {
        raw.clear();
    }
    private boolean canCraft(List<? extends StackedContents.IngredientInfo<Holder<Item>>> contents, int amount, StackedContents.Output<Holder<Item>> output) {
        return raw.tryPick(contents, amount, output);
    }
}
