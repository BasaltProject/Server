package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.EnderSignal;

public interface EnderSignalOps extends EnderSignal, Cored {
    default org.bukkit.inventory.ItemStack getItem() {
        java.util.Map<String, Object> stored = org.basaltmc.bukkit.item.Nbt.map(EntityNbt.compound(core(), "")
            .get("Item"));
        org.bukkit.inventory.ItemStack stack = stored == null ? null : org.basaltmc.bukkit.item.Convert.stack(stored);
        return stack != null ? stack : new org.bukkit.inventory.ItemStack(org.bukkit.Material.ENDER_EYE);
    }

    default void setItem(org.bukkit.inventory.ItemStack item) {
        if (item == null || item.getType().isAir()) {
            core().nbtRemove("Item");
        } else {
            EntityNbt.put(core(), "Item", org.basaltmc.bukkit.item.Convert.stack(item));
        }
    }
}
