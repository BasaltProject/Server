package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.AbstractNautilus;

public interface AbstractNautilusOps extends AbstractNautilus, Cored {
    default org.bukkit.inventory.ArmoredSaddledMountInventory getInventory() {
        return new org.basaltmc.bukkit.inv.NautilusInventory(core(), this);
    }
}
