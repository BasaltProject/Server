// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import java.util.Optional;
import net.minecraft.core.NonNullList;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.item.ItemStack;
public class RecipeCache {
    private final Entry[] entries;
    public RecipeCache(int capacity) {
        this.entries = new Entry[capacity];
    }
    public Optional<RecipeHolder<CraftingRecipe>> get(ServerLevel level, CraftingInput input) {
        if (input.isEmpty()) {
            return Optional.empty();
        }
        for (int i = 0; i < entries.length; i++) {
            Entry entry = entries[i];
            if (entry != null && entry.matches(input)) {
                moveToFront(i);
                return Optional.ofNullable(entry.value());
            }
        }
        Optional<RecipeHolder<CraftingRecipe>> found = level.recipeAccess().getRecipeFor(RecipeType.CRAFTING, input, level);
        insert(new Entry(NonNullList.of(ItemStack.EMPTY, input.items().toArray(ItemStack[]::new)), input.width(), input.height(), found.orElse(null)));
        return found;
    }
    static record Entry(NonNullList<ItemStack> key, int width, int height, RecipeHolder<CraftingRecipe> value) {
        public boolean matches(CraftingInput input) {
            if (width != input.width() || height != input.height()) {
            return false;
        }
        for (int i = 0; i < key.size(); i++) {
            if (!ItemStack.isSameItemSameComponents(key.get(i), input.getItem(i))) {
                return false;
            }
        }
        return true;
        }
    }
    private void moveToFront(int index) {
        if (index > 0) {
            Entry hit = entries[index];
            System.arraycopy(entries, 0, entries, 1, index);
            entries[0] = hit;
        }
    }
    private void insert(Entry entry) {
        if (entries.length > 0) {
            System.arraycopy(entries, 0, entries, 1, entries.length - 1);
            entries[0] = entry;
        }
    }
}
