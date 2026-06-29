package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface StorageMinecartOps extends org.bukkit.entity.minecart.StorageMinecart, Cored {
    default org.bukkit.inventory.Inventory getInventory() {
        return ContainerEntityOps.of(core(), this);
    }
}
