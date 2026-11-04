// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import net.minecraft.world.item.ItemStack;
public record BrewingInput(ItemStack input, ItemStack reagent) implements RecipeInput {
    public ItemStack getItem(int index) {
        return index == 0 ? input : reagent;
    }
    public int size() {
        return 2;
    }
}
