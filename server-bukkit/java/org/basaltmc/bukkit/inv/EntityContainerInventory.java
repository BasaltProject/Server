package org.basaltmc.bukkit.inv;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.entity.EntityCore;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.entity.Entity;
import org.bukkit.entity.HumanEntity;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

/** The chest of a chest minecart, chest boat or hopper minecart, kept in the vehicle's own data. */
public class EntityContainerInventory extends AbstractInventory {
    protected final EntityCore core;
    protected final Entity holder;
    private final int size;
    private final String key;

    public EntityContainerInventory(EntityCore core, Entity holder, int size) {
        this(core, holder, size, "Items");
    }

    public EntityContainerInventory(EntityCore core, Entity holder, int size, String key) {
        this.core = core;
        this.holder = holder;
        this.size = size;
        this.key = key;
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

    private Map<String, Object> saved() {
        byte[] stored = core.nbt("");
        Map<String, Object> read = stored == null ? null : Nbt.readNamed(stored);
        return read == null ? Nbt.compound() : read;
    }

    @Override
    public ItemStack getItem(int slot) {
        check(slot);
        List<Object> items = Nbt.list(saved().get(key));
        for (Object entry : items == null ? List.of() : items) {
            Map<String, Object> item = Nbt.map(entry);
            if (item != null && Nbt.integer(item, "Slot", -1) == slot) {
                ItemStack stack = Convert.stack(item);
                return stack instanceof BasaltItemStack own ? own.bound(stored -> setItem(slot, stored)) : stack;
            }
        }
        return null;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        check(slot);
        boolean empty = item == null || item.getType().isAir() || item.getAmount() <= 0;
        List<Object> items = Nbt.list(saved().get(key));
        List<Object> kept = new ArrayList<>();
        for (Object entry : items == null ? List.of() : items) {
            Map<String, Object> existing = Nbt.map(entry);
            if (existing != null && Nbt.integer(existing, "Slot", -1) != slot) {
                kept.add(existing);
            }
        }
        if (!empty) {
            Map<String, Object> stored = Convert.stack(item);
            stored.put("Slot", (byte) slot);
            kept.add(stored);
        }
        Map<String, Object> patch = Nbt.compound();
        patch.put(key, kept);
        core.mergeNbt(Nbt.writeNamed(patch));
    }

    @Override
    public List<HumanEntity> getViewers() {
        return new ArrayList<>();
    }

    @Override
    public InventoryType getType() {
        return size == 5 ? InventoryType.HOPPER : InventoryType.CHEST;
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
