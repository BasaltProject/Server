// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import java.util.List;
import net.minecraft.core.NonNullList;
import net.minecraft.world.entity.player.StackedItemContents;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.CraftingInput;

public class TransientCraftingContainer implements CraftingContainer {
    private final NonNullList<ItemStack> items;
    private final int width;
    private final int height;
    private final AbstractContainerMenu menu;

    public TransientCraftingContainer(AbstractContainerMenu menu, int width, int height) {
        this.menu = menu;
        this.width = width;
        this.height = height;
        this.items = NonNullList.withSize(width * height, ItemStack.EMPTY);
    }

    @Override
    public int getContainerSize() {
        return items.size();
    }

    @Override
    public boolean isEmpty() {
        for (ItemStack stack : items) {
            if (!stack.isEmpty()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public ItemStack getItem(int slot) {
        return slot >= getContainerSize() ? ItemStack.EMPTY : items.get(slot);
    }

    @Override
    public ItemStack removeItemNoUpdate(int slot) {
        ItemStack taken = items.get(slot);
        items.set(slot, ItemStack.EMPTY);
        return taken;
    }

    @Override
    public ItemStack removeItem(int slot, int count) {
        ItemStack current = items.get(slot);
        ItemStack taken = current.isEmpty() ? ItemStack.EMPTY : current.split(count);
        if (!taken.isEmpty()) {
            menu.slotsChanged(this);
        }
        return taken;
    }

    @Override
    public void setItem(int slot, ItemStack itemStack) {
        items.set(slot, itemStack);
        menu.slotsChanged(this);
    }

    @Override
    public void setChanged() {
    }

    @Override
    public boolean stillValid(Player player) {
        return true;
    }

    @Override
    public void clearContent() {
        items.clear();
        for (int i = 0; i < width * height; i++) {
            items.add(ItemStack.EMPTY);
        }
    }

    @Override
    public int getHeight() {
        return height;
    }

    @Override
    public int getWidth() {
        return width;
    }

    @Override
    public List<ItemStack> getItems() {
        return List.copyOf(items);
    }

    @Override
    public void fillStackedContents(StackedItemContents contents) {
    }
}
