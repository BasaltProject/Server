package org.basaltmc.bukkit.player;

import org.basaltmc.bukkit.BasaltItemStack;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.inv.AbstractInventory;
import org.bukkit.Location;
import org.bukkit.entity.HumanEntity;
import org.bukkit.entity.Player;
import org.bukkit.event.inventory.InventoryType;
import org.bukkit.inventory.InventoryHolder;
import org.bukkit.inventory.ItemStack;

final class PlayerEnderChest extends AbstractInventory {
    private final Player owner;

    PlayerEnderChest(Player owner) {
        this.owner = owner;
    }

    @Override
    public int getSize() {
        return PlayerActions.enderChestSize();
    }

    @Override
    public ItemStack getItem(int slot) {
        check(slot);
        Object held = PlayerCalls.call(owner.getUniqueId(), "ender_slot", (long) slot);
        ItemStack found = PlayerActions.stack(held);
        return found instanceof BasaltItemStack own ? own.bound(stored -> setItem(slot, stored)) : found;
    }

    @Override
    public void setItem(int slot, ItemStack item) {
        check(slot);
        boolean blank = item == null || item.getType().isAir() || item.getAmount() <= 0;
        PlayerCalls.call(owner.getUniqueId(), "set_ender_slot", (long) slot, blank ? "" : Items.toBytes(item));
        InventoryNatives.windowResync(owner.getUniqueId().toString());
    }

    private void check(int slot) {
        if (slot < 0 || slot >= getSize()) {
            throw new IndexOutOfBoundsException("Slot " + slot + " is outside of " + getSize());
        }
    }

    @Override
    public java.util.List<HumanEntity> getViewers() {
        return java.util.List.of();
    }

    @Override
    public InventoryType getType() {
        return InventoryType.ENDER_CHEST;
    }

    @Override
    public InventoryHolder getHolder() {
        return owner;
    }

    @Override
    public InventoryHolder getHolder(boolean useSnapshot) {
        return owner;
    }

    @Override
    public Location getLocation() {
        return null;
    }
}
