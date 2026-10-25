package org.basaltmc.bukkit.inv;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.entity.EntityCore;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

public class HeldInventory extends AbstractInventory {
    private final EntityCore core;
    private final Entity holder;
    private final String kind;
    private final int size;

    public HeldInventory(EntityCore core, Entity holder, String kind, int size) {
        this.core = core;
        this.holder = holder;
        this.kind = kind;
        this.size = size;
    }

    @Override
    public int getSize() {
        return size;
    }

    private void check(int slot) {
        if (slot < 0 || slot >= size) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + size);
        }
    }

    @Override
    public ItemStack getItem(int slot) {
        check(slot);
        if (!(core.call("held_get", kind, (long) slot) instanceof byte[] bytes)) {
            return null;
        }
        Map<String, Object> stored = Nbt.readNamed(bytes);
        ItemStack stack = stored == null ? null : Convert.stack(stored);
        return stack instanceof BasaltItemStack own ? own.bound(changed -> setItem(slot, changed)) : stack;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        check(slot);
        boolean empty = item == null || item.getType().isAir() || item.getAmount() <= 0;
        core.call("held_set", kind, (long) slot, empty ? null : Nbt.writeNamed(Convert.stack(item)));
    }

    @Override
    public List<HumanEntity> getViewers() {
        return new ArrayList<>();
    }

    @Override
    public InventoryType getType() {
        return InventoryType.CHEST;
    }

    @Override
    public InventoryHolder getHolder() {
        return (InventoryHolder) holder;
    }

    @Override
    public InventoryHolder getHolder(boolean useSnapshot) {
        return getHolder();
    }

    @Override
    public Location getLocation() {
        return holder.getLocation();
    }
}
