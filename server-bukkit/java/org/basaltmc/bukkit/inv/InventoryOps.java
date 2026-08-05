package org.basaltmc.bukkit.inv;

import java.util.HashMap;
import org.bukkit.Material;
import org.bukkit.inventory.Inventory;
import org.bukkit.inventory.ItemStack;

public final class InventoryOps {
    private InventoryOps() {}

    private static boolean blank(ItemStack item) {
        return item == null || item.getType().isAir() || item.getAmount() <= 0;
    }

    private static int limit(Inventory inv, ItemStack item) {
        return Math.max(1, Math.min(item.getMaxStackSize(), inv.getMaxStackSize()));
    }

    public static HashMap<Integer, ItemStack> add(Inventory inv, int slots, ItemStack[] items) {
        HashMap<Integer, ItemStack> left = new HashMap<>();
        for (int at = 0; at < items.length; at++) {
            ItemStack item = items[at];
            if (item == null) {
                throw new IllegalArgumentException("Item " + at + " cannot be null");
            }
            if (blank(item)) {
                continue;
            }
            ItemStack remaining = item.clone();
            int max = limit(inv, remaining);
            for (int slot = 0; slot < slots && remaining.getAmount() > 0; slot++) {
                ItemStack there = inv.getItem(slot);
                if (!blank(there) && there.isSimilar(remaining) && there.getAmount() < max) {
                    int moved = Math.min(max - there.getAmount(), remaining.getAmount());
                    ItemStack grown = there.clone();
                    grown.setAmount(there.getAmount() + moved);
                    inv.setItem(slot, grown);
                    remaining.setAmount(remaining.getAmount() - moved);
                }
            }
            for (int slot = 0; slot < slots && remaining.getAmount() > 0; slot++) {
                if (blank(inv.getItem(slot))) {
                    int moved = Math.min(max, remaining.getAmount());
                    ItemStack placed = remaining.clone();
                    placed.setAmount(moved);
                    inv.setItem(slot, placed);
                    remaining.setAmount(remaining.getAmount() - moved);
                }
            }
            if (remaining.getAmount() > 0) {
                left.put(at, remaining);
            }
        }
        return left;
    }

    public static HashMap<Integer, ItemStack> remove(Inventory inv, int slots, ItemStack[] items) {
        HashMap<Integer, ItemStack> left = new HashMap<>();
        for (int at = 0; at < items.length; at++) {
            ItemStack item = items[at];
            if (item == null) {
                throw new IllegalArgumentException("Item " + at + " cannot be null");
            }
            int toTake = item.getAmount();
            for (int slot = 0; slot < slots && toTake > 0; slot++) {
                ItemStack there = inv.getItem(slot);
                if (!blank(there) && there.isSimilar(item)) {
                    int taken = Math.min(there.getAmount(), toTake);
                    toTake -= taken;
                    if (taken == there.getAmount()) {
                        inv.setItem(slot, null);
                    } else {
                        ItemStack shrunk = there.clone();
                        shrunk.setAmount(there.getAmount() - taken);
                        inv.setItem(slot, shrunk);
                    }
                }
            }
            if (toTake > 0) {
                ItemStack rest = item.clone();
                rest.setAmount(toTake);
                left.put(at, rest);
            }
        }
        return left;
    }

    public static int count(Inventory inv, Material material) {
        int total = 0;
        for (int slot = 0; slot < inv.getSize(); slot++) {
            ItemStack there = inv.getItem(slot);
            if (!blank(there) && there.getType() == material) {
                total += there.getAmount();
            }
        }
        return total;
    }

    public static int countSimilar(Inventory inv, ItemStack item) {
        int total = 0;
        for (int slot = 0; slot < inv.getSize(); slot++) {
            ItemStack there = inv.getItem(slot);
            if (!blank(there) && there.isSimilar(item)) {
                total += there.getAmount();
            }
        }
        return total;
    }

    public static boolean contains(Inventory inv, Material material, int amount) {
        if (material == null) {
            throw new IllegalArgumentException("Material cannot be null");
        }
        return count(inv, material) >= amount;
    }

    public static boolean contains(Inventory inv, ItemStack item, int amount) {
        return item != null && countSimilar(inv, item) >= amount;
    }

    public static boolean contains(Inventory inv, ItemStack item) {
        if (item == null) {
            return false;
        }
        for (int slot = 0; slot < inv.getSize(); slot++) {
            if (item.equals(inv.getItem(slot))) {
                return true;
            }
        }
        return false;
    }

    public static HashMap<Integer, ItemStack> all(Inventory inv, Material material) {
        if (material == null) {
            throw new IllegalArgumentException("Material cannot be null");
        }
        HashMap<Integer, ItemStack> found = new HashMap<>();
        for (int slot = 0; slot < inv.getSize(); slot++) {
            ItemStack there = inv.getItem(slot);
            if (!blank(there) && there.getType() == material) {
                found.put(slot, there);
            }
        }
        return found;
    }

    public static HashMap<Integer, ItemStack> all(Inventory inv, ItemStack item) {
        HashMap<Integer, ItemStack> found = new HashMap<>();
        if (item != null) {
            for (int slot = 0; slot < inv.getSize(); slot++) {
                ItemStack there = inv.getItem(slot);
                if (item.equals(there)) {
                    found.put(slot, there);
                }
            }
        }
        return found;
    }

    public static int first(Inventory inv, Material material) {
        if (material == null) {
            throw new IllegalArgumentException("Material cannot be null");
        }
        for (int slot = 0; slot < inv.getSize(); slot++) {
            ItemStack there = inv.getItem(slot);
            if (!blank(there) && there.getType() == material) {
                return slot;
            }
        }
        return -1;
    }

    public static int first(Inventory inv, ItemStack item) {
        if (item == null) {
            return -1;
        }
        for (int slot = 0; slot < inv.getSize(); slot++) {
            if (item.equals(inv.getItem(slot))) {
                return slot;
            }
        }
        return -1;
    }

    public static int firstEmpty(Inventory inv, int slots) {
        for (int slot = 0; slot < slots; slot++) {
            if (blank(inv.getItem(slot))) {
                return slot;
            }
        }
        return -1;
    }

    public static boolean isEmpty(Inventory inv) {
        for (int slot = 0; slot < inv.getSize(); slot++) {
            if (!blank(inv.getItem(slot))) {
                return false;
            }
        }
        return true;
    }

    public static void remove(Inventory inv, Material material) {
        if (material == null) {
            throw new IllegalArgumentException("Material cannot be null");
        }
        for (int slot = 0; slot < inv.getSize(); slot++) {
            ItemStack there = inv.getItem(slot);
            if (!blank(there) && there.getType() == material) {
                inv.clear(slot);
            }
        }
    }

    public static void remove(Inventory inv, ItemStack item) {
        for (int slot = 0; slot < inv.getSize(); slot++) {
            ItemStack there = inv.getItem(slot);
            if (!blank(there) && there.equals(item)) {
                inv.clear(slot);
            }
        }
    }

    public static ItemStack[] contents(Inventory inv) {
        ItemStack[] all = new ItemStack[inv.getSize()];
        for (int slot = 0; slot < all.length; slot++) {
            all[slot] = inv.getItem(slot);
        }
        return all;
    }

    public static void setContents(Inventory inv, ItemStack[] items) {
        if (items.length > inv.getSize()) {
            throw new IllegalArgumentException("Invalid inventory size (" + items.length + "); expected " + inv.getSize() + " or less");
        }
        for (int slot = 0; slot < inv.getSize(); slot++) {
            inv.setItem(slot, slot < items.length ? items[slot] : null);
        }
    }
}
