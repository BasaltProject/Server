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
import org.basaltmc.bukkit.inv.InventoryOps;
import org.basaltmc.bukkit.player.PlayerCalls;

/** Slots: 0-8 hotbar, 9-35 rest, 36-39 armor boots up, 40 off hand. */
public final class BasaltPlayerInventory extends PlayerInventoryBase {
    private static final int kSize = 41;

    private final BasaltPlayer owner;
    private final String uuid;

    BasaltPlayerInventory(BasaltPlayer owner, String uuid) {
        this.owner = owner;
        this.uuid = uuid;
    }

    /** Bukkit counts the hotbar first (0 to 8) and the storage rows after it, armor boots first; Basalt keeps the rows first and armor head first. */
    private static int basaltSlot(int slot) {
        if (slot >= 0 && slot < 9) {
            return slot + 27;
        }
        if (slot >= 36 && slot < 40) {
            return 75 - slot;
        }
        return slot >= 9 && slot < 36 ? slot - 9 : slot;
    }

    @Override
    public int getSize() {
        return kSize;
    }

    @Override
    public int getMaxStackSize() {
        return (int) PlayerCalls.integer(java.util.UUID.fromString(uuid), "slot_limit");
    }

    @Override
    public void setMaxStackSize(int size) {
        PlayerCalls.set(java.util.UUID.fromString(uuid), "slot_limit", (long) Math.max(1, size));
    }

    @Override
    public ItemStack getItem(int slot) {
        ItemStack found = Items.fromBytes(InventoryNatives.playerItem(uuid, basaltSlot(slot)));
        return found instanceof BasaltItemStack own ? own.bound(stored -> setItem(slot, stored)) : found;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        InventoryNatives.setPlayerItem(uuid, basaltSlot(slot), Items.toBytes(item));
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
        if (slot < 0 || slot > 8) {
            throw new IllegalArgumentException("Slot must be between 0 and 8, got " + slot);
        }
        InventoryNatives.setHeldSlot(uuid, slot);
    }

    @Override
    public HashMap<Integer, ItemStack> addItem(ItemStack... items) {
        return InventoryOps.add(this, 36, items);
    }

    @Override
    public HashMap<Integer, ItemStack> removeItem(ItemStack... items) {
        return InventoryOps.remove(this, 36, items);
    }

    @Override
    public HashMap<Integer, ItemStack> removeItemAnySlot(ItemStack... items) {
        return InventoryOps.remove(this, kSize, items);
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
        return InventoryOps.firstEmpty(this, 36);
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
