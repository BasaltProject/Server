// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;

import net.minecraft.network.chat.Component;
import net.minecraft.world.Container;
import net.minecraft.world.Nameable;
import net.minecraft.world.item.ItemStack;
import org.bukkit.craftbukkit.inventory.CraftItemStack;

/** The player's slots as a container, backed by the live Bukkit player inventory (0-35 storage, 36-39 armor, 40 offhand). */
public class Inventory implements Container, Nameable {
    public static final int POP_TIME_DURATION = 5;
    public static final int INVENTORY_SIZE = 36;
    public static final int SELECTION_SIZE = 9;
    public static final int SLOT_OFFHAND = 40;
    public static final int NOT_FOUND_INDEX = -1;
    public final Player player;
    private final org.bukkit.inventory.PlayerInventory backing;

    public Inventory(Player player, org.bukkit.inventory.PlayerInventory backing) {
        this.player = player;
        this.backing = backing;
    }

    public int getSelectedSlot() {
        return backing.getHeldItemSlot();
    }

    public void setSelectedSlot(int selected) {
        backing.setHeldItemSlot(selected);
    }

    public ItemStack getSelectedItem() {
        return getItem(getSelectedSlot());
    }

    public boolean add(ItemStack stack) {
        for (int slot = 0; slot < INVENTORY_SIZE && !stack.isEmpty(); slot++) {
            ItemStack current = getItem(slot);
            if (!current.isEmpty() && ItemStack.isSameItemSameComponents(current, stack) && current.getCount() < current.getMaxStackSize()) {
                int moved = Math.min(stack.getCount(), current.getMaxStackSize() - current.getCount());
                current.grow(moved);
                stack.shrink(moved);
                setItem(slot, current);
            }
        }
        for (int slot = 0; slot < INVENTORY_SIZE && !stack.isEmpty(); slot++) {
            if (getItem(slot).isEmpty()) {
                setItem(slot, stack.copy());
                stack.setCount(0);
            }
        }
        return stack.isEmpty();
    }

    @Override
    public int getContainerSize() {
        return 41;
    }

    @Override
    public boolean isEmpty() {
        for (int slot = 0; slot < getContainerSize(); slot++) {
            if (!getItem(slot).isEmpty()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public ItemStack getItem(int slot) {
        return CraftItemStack.asNMSCopy(backing.getItem(slot));
    }

    @Override
    public ItemStack removeItem(int slot, int count) {
        ItemStack current = getItem(slot);
        if (current.isEmpty() || count <= 0) {
            return ItemStack.EMPTY;
        }
        ItemStack taken = current.split(count);
        setItem(slot, current);
        return taken;
    }

    @Override
    public ItemStack removeItemNoUpdate(int slot) {
        ItemStack current = getItem(slot);
        backing.setItem(slot, null);
        return current;
    }

    @Override
    public void setItem(int slot, ItemStack itemStack) {
        backing.setItem(slot, itemStack.isEmpty() ? null : CraftItemStack.asCraftMirror(itemStack));
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
        backing.clear();
    }

    @Override
    public Component getName() {
        return Component.translatable("container.inventory");
    }

    @Override
    public Component getDisplayName() {
        return getName();
    }
    public static boolean isUsableForCrafting(ItemStack itemStack) {
        return !itemStack.isDamaged() && !itemStack.isEnchanted() && !itemStack.has(net.minecraft.core.component.DataComponents.CUSTOM_NAME);
    }
}
