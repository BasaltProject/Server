package org.bukkit.craftbukkit.entity;

import net.minecraft.world.entity.Entity;
import org.basaltmc.bukkit.EntityBridge;

public abstract class CraftEntity {
    private volatile Entity handle;

    protected CraftEntity() {
    }

    public Entity getHandle() {
        Entity cached = handle;
        if (cached != null && !cached.isRemoved()) {
            return cached;
        }
        if (!(this instanceof org.bukkit.entity.Entity bukkit)) {
            throw new IllegalStateException("A craft entity must also be a Bukkit entity");
        }
        Entity made = EntityBridge.handle(bukkit.getWorld(), bukkit.getEntityId(), bukkit.getUniqueId(), bukkit.getType().key().asString());
        made.bindBukkitEntity(bukkit);
        handle = made;
        return made;
    }
}
