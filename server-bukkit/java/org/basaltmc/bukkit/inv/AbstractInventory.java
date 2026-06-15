package org.basaltmc.bukkit.inv;

import java.util.HashMap;
import java.util.List;
import java.util.ListIterator;
import org.basaltmc.bukkit.gen.InventoryBase;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.entity.HumanEntity;
import org.bukkit.inventory.ItemStack;

/** An inventory that only says how to read and write one slot; the rest follows from that. */
public abstract class AbstractInventory extends InventoryBase {
    private int maxStack = 64;

    protected int storageSize() {
        return getSize();
    }

    @Override
    public int getMaxStackSize() {
        return maxStack;
    }

    @Override
    public void setMaxStackSize(int size) {
        maxStack = size;
    }

    @Override
    public HashMap<Integer, ItemStack> addItem(ItemStack... items) {
        return InventoryOps.add(this, storageSize(), items);
    }

    @Override
    public HashMap<Integer, ItemStack> removeItem(ItemStack... items) {
        return InventoryOps.remove(this, storageSize(), items);
    }

    @Override
    public HashMap<Integer, ItemStack> removeItemAnySlot(ItemStack... items) {
        return InventoryOps.remove(this, getSize(), items);
    }

    @Override
    public ItemStack[] getContents() {
        return InventoryOps.contents(this);
    }

    @Override
    public void setContents(ItemStack[] items) {
        InventoryOps.setContents(this, items);
    }

    @Override
    public ItemStack[] getStorageContents() {
        return getContents();
    }

    @Override
    public void setStorageContents(ItemStack[] items) {
        setContents(items);
    }

    @Override
    public boolean contains(Material material) {
        return InventoryOps.first(this, material) >= 0;
    }

    @Override
    public boolean contains(ItemStack item) {
        return InventoryOps.contains(this, item);
    }

    @Override
    public boolean contains(Material material, int amount) {
        return InventoryOps.contains(this, material, amount);
    }

    @Override
    public boolean contains(ItemStack item, int amount) {
        return InventoryOps.contains(this, item, amount);
    }

    @Override
    public boolean containsAtLeast(ItemStack item, int amount) {
        return InventoryOps.contains(this, item, amount);
    }

    @Override
    public HashMap<Integer, ? extends ItemStack> all(Material material) {
        return InventoryOps.all(this, material);
    }

    @Override
    public HashMap<Integer, ? extends ItemStack> all(ItemStack item) {
        return InventoryOps.all(this, item);
    }

    @Override
    public int first(Material material) {
        return InventoryOps.first(this, material);
    }

    @Override
    public int first(ItemStack item) {
        return InventoryOps.first(this, item);
    }

    @Override
    public int firstEmpty() {
        return InventoryOps.firstEmpty(this, storageSize());
    }

    @Override
    public boolean isEmpty() {
        return InventoryOps.isEmpty(this);
    }

    @Override
    public void remove(Material material) {
        InventoryOps.remove(this, material);
    }

    @Override
    public void remove(ItemStack item) {
        InventoryOps.remove(this, item);
    }

    @Override
    public void clear(int slot) {
        setItem(slot, null);
    }

    @Override
    public void clear() {
        for (int slot = 0; slot < getSize(); slot++) {
            setItem(slot, null);
        }
    }

    @Override
    public int close() {
        List<HumanEntity> viewers = List.copyOf(getViewers());
        for (HumanEntity viewer : viewers) {
            viewer.closeInventory();
        }
        return viewers.size();
    }

    @Override
    public ListIterator<ItemStack> iterator() {
        return java.util.Arrays.asList(getContents()).listIterator();
    }

    @Override
    public ListIterator<ItemStack> iterator(int index) {
        if (index < 0) {
            index += getSize() + 1;
        }
        return java.util.Arrays.asList(getContents()).listIterator(index);
    }

    @Override
    public Location getLocation() {
        return null;
    }
}
