package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.ThrowableProjectile;

public interface ThrowableProjectileOps extends ThrowableProjectile, Cored {
    default org.bukkit.inventory.ItemStack getItem() {
        return EntityNbt.item(core(), "Item", org.bukkit.Material.AIR);
    }

    default void setItem(org.bukkit.inventory.ItemStack item) {
        EntityNbt.storeItem(core(), "Item", item);
    }
}
