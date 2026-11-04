// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import net.minecraft.world.item.ItemStack;
public record SingleRecipeInput(ItemStack item) implements RecipeInput {
    public ItemStack getItem(int index) {
        if (index != 0) {
            throw new IllegalArgumentException("No item for index " + index);
        }
        return item;
    }
    public int size() {
        return 1;
    }
}
