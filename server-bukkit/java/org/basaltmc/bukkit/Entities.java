package org.basaltmc.bukkit;

import org.bukkit.World;
import org.bukkit.entity.Entity;

/** Entities by the id the server gave them, for add-ons that read and spawn entities through native services. */
public final class Entities {
    private Entities() {}

    public static Entity byId(World world, int id) {
        return world instanceof BasaltWorld basalt ? basalt.entity(id) : null;
    }
}
