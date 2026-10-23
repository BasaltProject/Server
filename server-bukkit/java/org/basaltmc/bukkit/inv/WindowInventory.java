package org.basaltmc.bukkit.inv;

import java.util.ArrayList;
import java.util.List;
import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.Items;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

/** The top half of a window the server opened, read and written through that window's own slots. */
public class WindowInventory extends AbstractInventory {
    protected final String uuid;
    protected final InventoryType type;
    protected final int size;
    private final Location location;

    public WindowInventory(String uuid, InventoryType type, int size, Location location) {
        this.uuid = uuid;
        this.type = type;
        this.size = size;
        this.location = location;
    }

    @Override
    public int getSize() {
        return size;
    }

    @Override
    public ItemStack getItem(int slot) {
        if (slot < 0 || slot >= size) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + size);
        }
        ItemStack found = Items.fromBytes(InventoryNatives.windowItem(uuid, slot));
        return found instanceof BasaltItemStack own ? own.bound(stored -> setItem(slot, stored)) : found;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        if (slot < 0 || slot >= size) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + size);
        }
        InventoryNatives.setWindowItem(uuid, slot, Items.toBytes(item));
    }

    @Override
    public List<HumanEntity> getViewers() {
        HumanEntity viewer = Bukkit.getPlayer(java.util.UUID.fromString(uuid));
        List<HumanEntity> out = new ArrayList<>();
        if (viewer != null) {
            out.add(viewer);
        }
        return out;
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

    @Override
    public Location getLocation() {
        return location;
    }
}
