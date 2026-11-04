// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.inventory;

import java.util.Optional;
import net.minecraft.resources.Identifier;
import net.minecraft.world.Container;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;

public class Slot {
    public final Container container;
    public int index;
    public final int x;
    public final int y;
    private final int slot;

    public Slot(Container container, int slot, int x, int y) {
        this.container = container;
        this.slot = slot;
        this.x = x;
        this.y = y;
    }

    public void onQuickCraft(ItemStack picked, ItemStack original) {
        int count = original.getCount() - picked.getCount();
        if (count > 0) {
            onQuickCraft(picked, count);
        }
    }

    protected void onQuickCraft(ItemStack picked, int count) {
    }

    protected void onSwapCraft(int count) {
    }

    protected void checkTakeAchievements(ItemStack carried) {
    }

    public void onTake(Player player, ItemStack carried) {
        setChanged();
    }

    public boolean mayPlace(ItemStack itemStack) {
        return true;
    }

    public ItemStack getItem() {
        return container.getItem(slot);
    }

    public boolean hasItem() {
        return !getItem().isEmpty();
    }

    public void setByPlayer(ItemStack itemStack) {
        setByPlayer(itemStack, getItem());
    }

    public void setByPlayer(ItemStack itemStack, ItemStack previous) {
        set(itemStack);
    }

    public void set(ItemStack itemStack) {
        container.setItem(slot, itemStack);
        setChanged();
    }

    public void setChanged() {
        container.setChanged();
    }

    public int getMaxStackSize() {
        return container.getMaxStackSize();
    }

    public int getMaxStackSize(ItemStack itemStack) {
        return Math.min(getMaxStackSize(), itemStack.getMaxStackSize());
    }

    public Identifier getNoItemIcon() {
        return null;
    }

    public ItemStack remove(int amount) {
        return container.removeItem(slot, amount);
    }

    public boolean mayPickup(Player player) {
        return true;
    }

    public boolean isActive() {
        return true;
    }

    public Optional<ItemStack> tryRemove(int amount, int maxAmount, Player player) {
        if (!mayPickup(player)) {
            return Optional.empty();
        }
        if (!allowModification(player) && maxAmount < getItem().getCount()) {
            return Optional.empty();
        }
        int taken = Math.min(amount, maxAmount);
        ItemStack removed = remove(taken);
        if (removed.isEmpty()) {
            return Optional.empty();
        }
        if (getItem().isEmpty()) {
            setByPlayer(ItemStack.EMPTY, removed);
        }
        return Optional.of(removed);
    }

    public ItemStack safeTake(int amount, int maxAmount, Player player) {
        Optional<ItemStack> removed = tryRemove(amount, maxAmount, player);
        removed.ifPresent(stack -> onTake(player, stack));
        return removed.orElse(ItemStack.EMPTY);
    }

    public ItemStack safeClone(Player player) {
        return getItem().copy();
    }

    public ItemStack safeInsert(ItemStack stack) {
        return safeInsert(stack, stack.getCount());
    }

    public ItemStack safeInsert(ItemStack inputStack, int inputAmount) {
        if (inputStack.isEmpty() || !mayPlace(inputStack)) {
            return inputStack;
        }
        ItemStack current = getItem();
        int room = Math.min(Math.min(inputAmount, inputStack.getCount()), getMaxStackSize(inputStack) - current.getCount());
        if (room <= 0) {
            return inputStack;
        }
        if (current.isEmpty()) {
            setByPlayer(inputStack.split(room));
        } else if (ItemStack.isSameItemSameComponents(current, inputStack)) {
            inputStack.shrink(room);
            current.grow(room);
            setByPlayer(current);
        }
        return inputStack;
    }

    public boolean allowModification(Player player) {
        return mayPickup(player) && mayPlace(getItem());
    }

    public int getContainerSlot() {
        return slot;
    }

    public boolean isHighlightable() {
        return true;
    }

    public boolean isFake() {
        return false;
    }
}
