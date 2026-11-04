// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import java.util.List;
import net.minecraft.world.entity.player.StackedItemContents;
import net.minecraft.world.item.ItemStack;
public class CraftingInput implements RecipeInput {
    private final int width;
    private final int height;
    private final List<ItemStack> items;
    private final int ingredientCount;
    public static final CraftingInput EMPTY = new CraftingInput(0, 0, List.of());
    private CraftingInput(int width, int height, List<ItemStack> items) {
        this.width = width;
        this.height = height;
        this.items = items;
        int count = 0;
        for (ItemStack stack : items) {
            if (!stack.isEmpty()) {
                count++;
            }
        }
        this.ingredientCount = count;
    }
    public static CraftingInput of(int width, int height, List<ItemStack> items) {
        return width != 0 && height != 0 ? new CraftingInput(width, height, items) : EMPTY;
    }
    public static Positioned ofPositioned(int width, int height, List<ItemStack> items) {
        int left = width;
        int right = 0;
        int top = height;
        int bottom = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (!items.get(x + y * width).isEmpty()) {
                    left = Math.min(left, x);
                    right = Math.max(right, x);
                    top = Math.min(top, y);
                    bottom = Math.max(bottom, y);
                }
            }
        }
        if (left > right) {
            return Positioned.EMPTY;
        }
        int newWidth = right - left + 1;
        int newHeight = bottom - top + 1;
        List<ItemStack> cut = new java.util.ArrayList<>();
        for (int y = top; y <= bottom; y++) {
            for (int x = left; x <= right; x++) {
                cut.add(items.get(x + y * width));
            }
        }
        return new Positioned(new CraftingInput(newWidth, newHeight, cut), left, top);
    }
    public ItemStack getItem(int index) {
        return items.get(index);
    }
    public ItemStack getItem(int x, int y) {
        return items.get(x + y * width);
    }
    public int size() {
        return items.size();
    }
    public boolean isEmpty() {
        return ingredientCount == 0;
    }
    public StackedItemContents stackedContents() {
        net.minecraft.world.entity.player.StackedItemContents contents = new net.minecraft.world.entity.player.StackedItemContents();
        for (ItemStack stack : items) {
            contents.accountSimpleStack(stack);
        }
        return contents;
    }
    public List<ItemStack> items() {
        return items;
    }
    public int ingredientCount() {
        return ingredientCount;
    }
    public int width() {
        return width;
    }
    public int height() {
        return height;
    }
    public boolean equals(Object obj) {
        return obj instanceof CraftingInput other && width == other.width && height == other.height
            && ItemStack.listMatches(items, other.items);
    }
    public int hashCode() {
        return 31 * (31 * width + height) + ItemStack.hashStackList(items);
    }
    public static record Positioned(CraftingInput input, int left, int top) {
        public static final Positioned EMPTY = new Positioned(CraftingInput.EMPTY, 0, 0);
    }
}
