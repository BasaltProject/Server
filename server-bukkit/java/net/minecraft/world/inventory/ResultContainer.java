// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class ResultContainer implements Container {
    private ItemStack itemStack = ItemStack.EMPTY;

    public ResultContainer() {}

    @Override
    public int getContainerSize() {
        return 1;
    }

    @Override
    public boolean isEmpty() {
        return itemStack.isEmpty();
    }

    @Override
    public ItemStack getItem(int slot) {
        return itemStack;
    }

    @Override
    public ItemStack removeItem(int slot, int count) {
        return itemStack.isEmpty() ? ItemStack.EMPTY : itemStack.split(count);
    }

    @Override
    public ItemStack removeItemNoUpdate(int slot) {
        ItemStack taken = itemStack;
        itemStack = ItemStack.EMPTY;
        return taken;
    }

    @Override
    public void setItem(int slot, ItemStack itemStack) {
        this.itemStack = itemStack;
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
        itemStack = ItemStack.EMPTY;
    }
}
