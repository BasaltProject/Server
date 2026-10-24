package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface PillagerOps extends org.bukkit.entity.Pillager, Cored {
    default org.bukkit.inventory.Inventory getInventory() {
        return new org.basaltmc.bukkit.inv.EntityContainerInventory(core(), this, 5, "Inventory");
    }
}
