package org.basaltmc.bukkit.inv;

import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.entity.Entity;
import org.bukkit.inventory.ArmoredSaddledMountInventory;

/** The saddle and the body armour of a nautilus, which are all it carries. */
public final class NautilusInventory extends MountInventory implements ArmoredSaddledMountInventory {
    public NautilusInventory(EntityCore core, Entity nautilus) {
        super(core, nautilus);
    }
}
