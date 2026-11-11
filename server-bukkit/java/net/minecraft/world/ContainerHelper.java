// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import java.util.List;
import java.util.function.Predicate;
import net.minecraft.core.NonNullList;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public class ContainerHelper {
    public static final String TAG_ITEMS = "Items";

    public static ItemStack removeItem(List<ItemStack> itemStacks, int slot, int count) {
        if (slot < 0 || slot >= itemStacks.size() || itemStacks.get(slot).isEmpty() || count <= 0) {
            return ItemStack.EMPTY;
        }
        return itemStacks.get(slot).split(count);
    }

    public static ItemStack takeItem(List<ItemStack> itemStacks, int slot) {
        if (slot < 0 || slot >= itemStacks.size()) {
            return ItemStack.EMPTY;
        }
        return itemStacks.set(slot, ItemStack.EMPTY);
    }

    public static void saveAllItems(ValueOutput output, NonNullList<ItemStack> itemStacks) {
        saveAllItems(output, itemStacks, true);
    }

    public static void saveAllItems(ValueOutput output, NonNullList<ItemStack> itemStacks, boolean alsoWhenEmpty) {
        ValueOutput.TypedOutputList<ItemStackWithSlot> out = output.list(TAG_ITEMS, ItemStackWithSlot.CODEC);
        for (int slot = 0; slot < itemStacks.size(); slot++) {
            if (!itemStacks.get(slot).isEmpty()) {
                out.add(new ItemStackWithSlot(slot, itemStacks.get(slot)));
            }
        }
        if (out.isEmpty() && !alsoWhenEmpty) {
            output.discard(TAG_ITEMS);
        }
    }

    public static void loadAllItems(ValueInput input, NonNullList<ItemStack> itemStacks) {
        for (ItemStackWithSlot entry : input.listOrEmpty(TAG_ITEMS, ItemStackWithSlot.CODEC)) {
            if (entry.isValidInContainer(itemStacks.size())) {
                itemStacks.set(entry.slot(), entry.stack());
            }
        }
    }

    public static int clearOrCountMatchingItems(Container container, Predicate<ItemStack> predicate, int amountToRemove, boolean countingOnly) {
        int found = 0;
        for (int slot = 0; slot < container.getContainerSize(); slot++) {
            ItemStack stack = container.getItem(slot);
            int taken = clearOrCountMatchingItems(stack, predicate, amountToRemove - found, countingOnly);
            if (taken > 0 && !countingOnly && stack.isEmpty()) {
                container.setItem(slot, ItemStack.EMPTY);
            }
            found += taken;
        }
        return found;
    }

    public static int clearOrCountMatchingItems(ItemStack itemStack, Predicate<ItemStack> predicate, int amountToRemove, boolean countingOnly) {
        if (itemStack.isEmpty() || !predicate.test(itemStack)) {
            return 0;
        }
        if (countingOnly) {
            return itemStack.getCount();
        }
        int removed = amountToRemove < 0 ? itemStack.getCount() : Math.min(amountToRemove, itemStack.getCount());
        itemStack.shrink(removed);
        return removed;
    }
}
