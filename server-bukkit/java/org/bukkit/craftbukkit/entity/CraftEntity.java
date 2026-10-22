package org.bukkit.craftbukkit.entity;

import net.minecraft.world.entity.Entity;
import org.basaltmc.bukkit.Unsupported;
public abstract class CraftEntity {
    protected CraftEntity() {
    }
    public Entity getHandle() {
        throw Unsupported.hit("org.bukkit.craftbukkit.entity.CraftEntity.getHandle");
    }
}
