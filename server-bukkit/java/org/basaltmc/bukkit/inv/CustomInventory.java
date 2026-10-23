package org.basaltmc.bukkit.inv;

import java.util.ArrayList;
import java.util.List;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.Items;
import org.bukkit.Location;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

/** An inventory a plugin made: its contents live here, and each window that shows it is kept in step. */
public class CustomInventory extends AbstractInventory {
    private final InventoryHolder holder;
    private final InventoryType type;
    private final Component title;
    private final ItemStack[] contents;
    private final List<HumanEntity> viewers = new ArrayList<>();

    public CustomInventory(InventoryHolder holder, InventoryType type, int size, Component title) {
        this.holder = holder;
        this.type = type;
        this.title = title == null ? type.defaultTitle() : title;
        this.contents = new ItemStack[size];
    }

    public Component title() {
        return title;
    }

    @Override
    public int getSize() {
        return contents.length;
    }

    @Override
    public ItemStack getItem(int slot) {
        ItemStack there = contents[slot];
        if (there == null || there.getType().isAir() || there.getAmount() <= 0) {
            return null;
        }
        ItemStack copy = there.clone();
        return copy instanceof BasaltItemStack own ? own.bound(stored -> setItem(slot, stored)) : copy;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        if (slot < 0 || slot >= contents.length) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + contents.length);
        }
        boolean blank = item == null || item.getType().isAir() || item.getAmount() <= 0;
        contents[slot] = blank ? null : item.clone();
        byte[] encoded = blank ? null : Items.toBytes(contents[slot]);
        for (HumanEntity viewer : List.copyOf(viewers)) {
            InventoryNatives.setWindowItem(viewer.getUniqueId().toString(), slot, encoded);
        }
    }

    /** Takes a slot's contents as the window now holds them, after a click moved something. */
    void absorb(int slot, ItemStack item) {
        contents[slot] = item == null || item.getType().isAir() || item.getAmount() <= 0 ? null : item;
    }

    ItemStack raw(int slot) {
        return contents[slot];
    }

    @Override
    public List<HumanEntity> getViewers() {
        return List.copyOf(viewers);
    }

    void attach(HumanEntity viewer) {
        if (!viewers.contains(viewer)) {
            viewers.add(viewer);
        }
    }

    void detach(HumanEntity viewer) {
        viewers.remove(viewer);
    }

    @Override
    public InventoryType getType() {
        return type;
    }

    @Override
    public InventoryHolder getHolder() {
        return holder;
    }

    @Override
    public InventoryHolder getHolder(boolean useSnapshot) {
        return holder;
    }

    @Override
    public Location getLocation() {
        return holder instanceof org.bukkit.block.BlockState state ? state.getLocation() : null;
    }
}
