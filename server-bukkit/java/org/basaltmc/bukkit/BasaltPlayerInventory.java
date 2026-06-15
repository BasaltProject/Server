package org.basaltmc.bukkit;

import java.util.HashMap;
import java.util.List;
import java.util.ListIterator;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;
import org.basaltmc.bukkit.gen.PlayerInventoryBase;

/** Slots: 0-8 hotbar, 9-35 rest, 36-39 armor boots up, 40 off hand. */
public final class BasaltPlayerInventory extends PlayerInventoryBase {
    private static final int kSize = 41;

    private final BasaltPlayer owner;
    private final String uuid;

    BasaltPlayerInventory(BasaltPlayer owner, String uuid) {
        this.owner = owner;
        this.uuid = uuid;
    }

    /** Bukkit counts the hotbar first (0 to 8) and the storage rows after it; Basalt keeps the rows first. */
    private static int basaltSlot(int slot) {
        if (slot >= 0 && slot < 9) {
            return slot + 27;
        }
        return slot >= 9 && slot < 36 ? slot - 9 : slot;
    }

    @Override
    public int getSize() {
        return kSize;
    }

    @Override
    public int getMaxStackSize() {
        return 99;
    }

    @Override
    public void setMaxStackSize(int size) {}

    @Override
    public ItemStack getItem(int slot) {
        return Items.fromStrings(WorldNatives.inventoryGet(uuid, basaltSlot(slot)));
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        WorldNatives.inventorySet(uuid, basaltSlot(slot), Items.toStrings(item));
    }

    @Override
    public ItemStack[] getContents() {
        ItemStack[] all = new ItemStack[kSize];
        for (int slot = 0; slot < kSize; slot++) {
            all[slot] = getItem(slot);
        }
        return all;
    }

    @Override
    public void setContents(ItemStack[] items) {
        for (int slot = 0; slot < kSize; slot++) {
            setItem(slot, slot < items.length ? items[slot] : null);
        }
    }

    @Override
    public ItemStack[] getStorageContents() {
        ItemStack[] main = new ItemStack[36];
        for (int slot = 0; slot < 36; slot++) {
            main[slot] = getItem(slot);
        }
        return main;
    }

    @Override
    public void setStorageContents(ItemStack[] items) {
        for (int slot = 0; slot < 36; slot++) {
            setItem(slot, slot < items.length ? items[slot] : null);
        }
    }

    @Override
    public ItemStack[] getArmorContents() {
        return new ItemStack[] {getItem(36), getItem(37), getItem(38), getItem(39)};
    }

    @Override
    public void setArmorContents(ItemStack[] items) {
        for (int i = 0; i < 4; i++) {
            setItem(36 + i, i < items.length ? items[i] : null);
        }
    }

    @Override
    public ItemStack[] getExtraContents() {
        return new ItemStack[] {getItem(40)};
    }

    @Override
    public void setExtraContents(ItemStack[] items) {
        setItem(40, items.length > 0 ? items[0] : null);
    }

    @Override
    public ItemStack getHelmet() {
        return getItem(39);
    }

    @Override
    public ItemStack getChestplate() {
        return getItem(38);
    }

    @Override
    public ItemStack getLeggings() {
        return getItem(37);
    }

    @Override
    public ItemStack getBoots() {
        return getItem(36);
    }

    @Override
    public void setHelmet(ItemStack item) {
        setItem(39, item);
    }

    @Override
    public void setChestplate(ItemStack item) {
        setItem(38, item);
    }

    @Override
    public void setLeggings(ItemStack item) {
        setItem(37, item);
    }

    @Override
    public void setBoots(ItemStack item) {
        setItem(36, item);
    }

    private static int slotOf(EquipmentSlot slot, int held) {
        return switch (slot) {
            case HAND -> held;
            case OFF_HAND -> 40;
            case FEET -> 36;
            case LEGS -> 37;
            case CHEST -> 38;
            case HEAD -> 39;
            default -> throw new IllegalArgumentException("No inventory slot for " + slot);
        };
    }

    @Override
    public ItemStack getItem(EquipmentSlot slot) {
        return getItem(slotOf(slot, getHeldItemSlot()));
    }

    @Override
    public void setItem(EquipmentSlot slot, ItemStack item) {
        setItem(slotOf(slot, getHeldItemSlot()), item);
    }

    @Override
    public ItemStack getItemInMainHand() {
        ItemStack held = getItem(getHeldItemSlot());
        return held == null ? new BasaltItemStack(org.bukkit.Material.AIR, 0) : held;
    }

    @Override
    public void setItemInMainHand(ItemStack item) {
        setItem(getHeldItemSlot(), item);
    }

    @Override
    public ItemStack getItemInOffHand() {
        ItemStack held = getItem(40);
        return held == null ? new BasaltItemStack(org.bukkit.Material.AIR, 0) : held;
    }

    @Override
    public void setItemInOffHand(ItemStack item) {
        setItem(40, item);
    }

    @Override
    public ItemStack getItemInHand() {
        return getItemInMainHand();
    }

    @Override
    public void setItemInHand(ItemStack item) {
        setItemInMainHand(item);
    }

    @Override
    public int getHeldItemSlot() {
        return WorldNatives.inventoryHeld(uuid);
    }

    @Override
    public void setHeldItemSlot(int slot) {
        throw Unsupported.hit("PlayerInventory#setHeldItemSlot");
    }

    @Override
    public HashMap<Integer, ItemStack> addItem(ItemStack... items) {
        HashMap<Integer, ItemStack> left = new HashMap<>();
        for (int i = 0; i < items.length; i++) {
            if (items[i] == null || items[i].getType().isAir() || items[i].getAmount() <= 0) {
                continue;
            }
            ItemStack remaining = items[i].clone();
            int max = Math.max(1, remaining.getMaxStackSize());
            for (int slot = 0; slot < 36 && remaining.getAmount() > 0; slot++) {
                ItemStack there = getItem(slot);
                if (there != null && there.isSimilar(remaining) && there.getAmount() < max) {
                    int moved = Math.min(max - there.getAmount(), remaining.getAmount());
                    there.setAmount(there.getAmount() + moved);
                    setItem(slot, there);
                    remaining.setAmount(remaining.getAmount() - moved);
                }
            }
            for (int slot = 0; slot < 36 && remaining.getAmount() > 0; slot++) {
                if (getItem(slot) == null) {
                    int moved = Math.min(max, remaining.getAmount());
                    ItemStack placed = remaining.clone();
                    placed.setAmount(moved);
                    setItem(slot, placed);
                    remaining.setAmount(remaining.getAmount() - moved);
                }
            }
            if (remaining.getAmount() > 0) {
                left.put(i, remaining);
            }
        }
        return left;
    }

    @Override
    public HashMap<Integer, ItemStack> removeItem(ItemStack... items) {
        HashMap<Integer, ItemStack> left = new HashMap<>();
        for (int i = 0; i < items.length; i++) {
            int toTake = items[i].getAmount();
            for (int slot = 0; slot < kSize && toTake > 0; slot++) {
                ItemStack there = getItem(slot);
                if (there != null && there.isSimilar(items[i])) {
                    int taken = Math.min(there.getAmount(), toTake);
                    toTake -= taken;
                    there.setAmount(there.getAmount() - taken);
                    setItem(slot, there.getAmount() > 0 ? there : null);
                }
            }
            if (toTake > 0) {
                ItemStack rest = items[i].clone();
                rest.setAmount(toTake);
                left.put(i, rest);
            }
        }
        return left;
    }

    @Override
    public HashMap<Integer, ItemStack> removeItemAnySlot(ItemStack... items) {
        return removeItem(items);
    }

    @Override
    public boolean contains(Material material) {
        return first(material) >= 0;
    }

    @Override
    public boolean contains(ItemStack item) {
        return first(item) >= 0;
    }

    @Override
    public boolean contains(Material material, int amount) {
        return count(material) >= amount;
    }

    @Override
    public boolean contains(ItemStack item, int amount) {
        return count(item.getType()) >= amount;
    }

    @Override
    public boolean containsAtLeast(ItemStack item, int amount) {
        return count(item.getType()) >= amount;
    }

    private int count(Material material) {
        int total = 0;
        for (int slot = 0; slot < kSize; slot++) {
            ItemStack there = getItem(slot);
            if (there != null && there.getType() == material) {
                total += there.getAmount();
            }
        }
        return total;
    }

    @Override
    public HashMap<Integer, ? extends ItemStack> all(Material material) {
        HashMap<Integer, ItemStack> found = new HashMap<>();
        for (int slot = 0; slot < kSize; slot++) {
            ItemStack there = getItem(slot);
            if (there != null && there.getType() == material) {
                found.put(slot, there);
            }
        }
        return found;
    }

    @Override
    public HashMap<Integer, ? extends ItemStack> all(ItemStack item) {
        HashMap<Integer, ItemStack> found = new HashMap<>();
        for (int slot = 0; slot < kSize; slot++) {
            ItemStack there = getItem(slot);
            if (there != null && there.equals(item)) {
                found.put(slot, there);
            }
        }
        return found;
    }

    @Override
    public int first(Material material) {
        for (int slot = 0; slot < kSize; slot++) {
            ItemStack there = getItem(slot);
            if (there != null && there.getType() == material) {
                return slot;
            }
        }
        return -1;
    }

    @Override
    public int first(ItemStack item) {
        for (int slot = 0; slot < kSize; slot++) {
            ItemStack there = getItem(slot);
            if (there != null && there.isSimilar(item)) {
                return slot;
            }
        }
        return -1;
    }

    @Override
    public int firstEmpty() {
        for (int slot = 0; slot < 36; slot++) {
            if (getItem(slot) == null) {
                return slot;
            }
        }
        return -1;
    }

    @Override
    public boolean isEmpty() {
        for (int slot = 0; slot < kSize; slot++) {
            if (getItem(slot) != null) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void remove(Material material) {
        for (int slot = 0; slot < kSize; slot++) {
            ItemStack there = getItem(slot);
            if (there != null && there.getType() == material) {
                setItem(slot, null);
            }
        }
    }

    @Override
    public void remove(ItemStack item) {
        for (int slot = 0; slot < kSize; slot++) {
            ItemStack there = getItem(slot);
            if (there != null && there.isSimilar(item)) {
                setItem(slot, null);
            }
        }
    }

    @Override
    public void clear(int slot) {
        setItem(slot, null);
    }

    @Override
    public void clear() {
        WorldNatives.inventoryClear(uuid);
    }

    @Override
    public int close() {
        return 0;
    }

    @Override
    public List<HumanEntity> getViewers() {
        return List.of();
    }

    @Override
    public InventoryType getType() {
        return InventoryType.PLAYER;
    }

    @Override
    public HumanEntity getHolder() {
        return owner;
    }

    @Override
    public InventoryHolder getHolder(boolean useSnapshot) {
        return owner;
    }

    @Override
    public ListIterator<ItemStack> iterator() {
        return java.util.Arrays.asList(getContents()).listIterator();
    }

    @Override
    public ListIterator<ItemStack> iterator(int index) {
        return java.util.Arrays.asList(getContents()).listIterator(index);
    }

    @Override
    public Location getLocation() {
        return owner.getLocation();
    }
}
