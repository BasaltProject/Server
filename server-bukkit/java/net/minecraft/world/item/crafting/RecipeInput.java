// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import net.minecraft.world.item.ItemStack;
public interface RecipeInput {
    ItemStack getItem(int index);
    int size();
    default boolean isEmpty() {
        for (int i = 0; i < size(); i++) {
            if (!getItem(i).isEmpty()) {
                return false;
            }
        }
        return true;
    }
}
