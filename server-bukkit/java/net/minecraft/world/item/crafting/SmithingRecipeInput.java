// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import net.minecraft.world.item.ItemStack;
public record SmithingRecipeInput(ItemStack template, ItemStack base, ItemStack addition) implements RecipeInput {
    public ItemStack getItem(int index) {
        return switch (index) {
            case 0 -> template;
            case 1 -> base;
            case 2 -> addition;
            default -> throw new IllegalArgumentException("No item for index " + index);
        };
    }
    public int size() {
        return 3;
    }
    public boolean isEmpty() {
        return template.isEmpty() && base.isEmpty() && addition.isEmpty();
    }
}
