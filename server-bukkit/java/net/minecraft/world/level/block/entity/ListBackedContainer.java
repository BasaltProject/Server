// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.NonNullList;
import net.minecraft.world.Container;
import net.minecraft.world.ContainerHelper;
import net.minecraft.world.item.ItemStack;

public interface ListBackedContainer extends Container {
    NonNullList<ItemStack> getItems();

    default int count() {
        int filled = 0;
        for (ItemStack stack : getItems()) {
            if (!stack.isEmpty()) {
                filled++;
            }
        }
        return filled;
    }

    default int getContainerSize() {
        return getItems().size();
    }

    default void clearContent() {
        getItems().clear();
    }

    default boolean isEmpty() {
        return count() == 0;
    }

    default ItemStack getItem(int slot) {
        return getItems().get(slot);
    }

    default ItemStack removeItem(int slot, int count) {
        ItemStack removed = ContainerHelper.removeItem(getItems(), slot, count);
        if (!removed.isEmpty()) {
            setChanged();
        }
        return removed;
    }

    default ItemStack removeItemNoUpdate(int slot) {
        return ContainerHelper.removeItem(getItems(), slot, getMaxStackSize());
    }

    default boolean canPlaceItem(int slot, ItemStack itemStack) {
        ItemStack held = getItem(slot);
        return acceptsItemType(itemStack) && (held.isEmpty() || held.getCount() < getMaxStackSize(itemStack));
    }

    default boolean acceptsItemType(ItemStack itemStack) {
        return true;
    }

    default void setItem(int slot, ItemStack itemStack) {
        setItemNoUpdate(slot, itemStack);
        setChanged();
    }

    default void setItemNoUpdate(int slot, ItemStack itemStack) {
        getItems().set(slot, itemStack);
        itemStack.limitSize(getMaxStackSize(itemStack));
    }
}
