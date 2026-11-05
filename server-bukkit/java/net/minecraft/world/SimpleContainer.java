// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;
import java.util.List;
import net.minecraft.core.NonNullList;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.player.StackedItemContents;
import net.minecraft.world.inventory.StackedContentsCompatible;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class SimpleContainer implements Container, StackedContentsCompatible {
    private final int size;
    private final NonNullList<ItemStack> items;
    private final java.util.List<java.lang.Runnable> listeners = new java.util.concurrent.CopyOnWriteArrayList<>();
    public SimpleContainer(int size) {
        this.size = size;
        this.items = NonNullList.withSize(size, ItemStack.EMPTY);
    }
    public SimpleContainer(ItemStack... itemstacks) {
        this(itemstacks.length);
        for (int i = 0; i < itemstacks.length; i++) {
            items.set(i, itemstacks[i]);
        }
    }
    public ItemStack getItem(int slot) {
        return slot >= 0 && slot < items.size() ? items.get(slot) : ItemStack.EMPTY;
    }
    public List<ItemStack> removeAllItems() {
        java.util.List<ItemStack> removed = items.stream().filter(stack -> !stack.isEmpty()).toList();
        clearContent();
        return removed;
    }
    public ItemStack removeItem(int slot, int count) {
        if (slot < 0 || slot >= items.size() || items.get(slot).isEmpty() || count <= 0) {
            return ItemStack.EMPTY;
        }
        ItemStack taken = items.get(slot).split(count);
        if (!taken.isEmpty()) {
            setChanged();
        }
        return taken;
    }
    public ItemStack removeItemType(Item itemType, int count) {
        ItemStack result = new ItemStack(itemType, 0);
        for (int i = size - 1; i >= 0; i--) {
            ItemStack stack = items.get(i);
            if (stack.getItem() == itemType) {
                int missing = count - result.getCount();
                ItemStack taken = stack.split(missing);
                result.grow(taken.getCount());
                if (result.getCount() == count) {
                    break;
                }
            }
        }
        if (!result.isEmpty()) {
            setChanged();
        }
        return result;
    }
    public ItemStack addItem(ItemStack itemStack) {
        if (itemStack.isEmpty()) {
            return ItemStack.EMPTY;
        }
        ItemStack rest = itemStack.copy();
        for (int pass = 0; pass < 2; pass++) {
            for (int i = 0; i < size && !rest.isEmpty(); i++) {
                ItemStack slot = items.get(i);
                if (pass == 0 && !slot.isEmpty() && ItemStack.isSameItemSameComponents(slot, rest)) {
                    int room = Math.min(getMaxStackSize(), slot.getMaxStackSize()) - slot.getCount();
                    int moved = Math.min(room, rest.getCount());
                    if (moved > 0) {
                        slot.grow(moved);
                        rest.shrink(moved);
                    }
                } else if (pass == 1 && slot.isEmpty()) {
                    int moved = Math.min(getMaxStackSize(), rest.getCount());
                    items.set(i, rest.split(moved));
                }
            }
        }
        setChanged();
        return rest.isEmpty() ? ItemStack.EMPTY : rest;
    }
    public boolean canAddItem(ItemStack itemStack) {
        for (ItemStack slot : items) {
            if (slot.isEmpty() || ItemStack.isSameItemSameComponents(slot, itemStack)
                && slot.getCount() < Math.min(getMaxStackSize(), slot.getMaxStackSize())) {
                return true;
            }
        }
        return false;
    }
    public ItemStack removeItemNoUpdate(int slot) {
        if (slot < 0 || slot >= items.size()) {
            return ItemStack.EMPTY;
        }
        ItemStack old = items.get(slot);
        items.set(slot, ItemStack.EMPTY);
        return old;
    }
    public void setItem(int slot, ItemStack itemStack) {
        if (slot < 0 || slot >= items.size()) {
            return;
        }
        items.set(slot, itemStack);
        itemStack.limitSize(getMaxStackSize(itemStack));
        setChanged();
    }
    public void setChanged() {
        for (java.lang.Runnable listener : listeners) {
            listener.run();
        }
    }
    public int getContainerSize() {
        return size;
    }
    public boolean isEmpty() {
        return items.stream().allMatch(ItemStack::isEmpty);
    }
    public boolean stillValid(Player player) {
        return true;
    }
    public void clearContent() {
        items.replaceAll(stack -> ItemStack.EMPTY);
    }
    public void fillStackedContents(StackedItemContents contents) {
        for (ItemStack stack : items) {
            contents.accountStack(stack);
        }
    }
    public String toString() {
        return items.stream().filter(stack -> !stack.isEmpty()).toList().toString();
    }
    public void fromItemList(ValueInput.TypedInputList<ItemStack> items) {
        clearContent();
        for (ItemStack stack : items) {
            addItem(stack);
        }
    }
    public void storeAsItemList(ValueOutput.TypedOutputList<ItemStack> output) {
        for (ItemStack stack : items) {
            if (!stack.isEmpty()) {
                output.add(stack);
            }
        }
    }
    public NonNullList<ItemStack> getItems() {
        return items;
    }
}
