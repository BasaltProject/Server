package org.basaltmc.bukkit.inv;

import java.util.List;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

/** The slots of a window as they stood at one moment, which an event's listeners may change before they are applied. */
public final class SnapshotInventory extends AbstractInventory {
    private final InventoryType type;
    private final ItemStack[] slots;
    private List<HumanEntity> viewers = List.of();

    public SnapshotInventory(InventoryType type, ItemStack[] slots) {
        this.type = type;
        this.slots = slots;
    }

    @Override
    public int getSize() {
        return slots.length;
    }

    @Override
    public ItemStack getItem(int slot) {
        ItemStack there = slots[slot];
        return there == null || there.getType().isAir() || there.getAmount() <= 0 ? null : there;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        slots[slot] = item;
    }

    public SnapshotInventory withViewers(List<HumanEntity> watching) {
        viewers = List.copyOf(watching);
        return this;
    }

    @Override
    public List<HumanEntity> getViewers() {
        return viewers;
    }

    @Override
    public InventoryType getType() {
        return type;
    }

    @Override
    public InventoryHolder getHolder() {
        return null;
    }

    @Override
    public InventoryHolder getHolder(boolean useSnapshot) {
        return null;
    }
}
