package org.basaltmc.bukkit.entity;

import org.bukkit.craftbukkit.entity.CraftEntity;
import org.bukkit.entity.Entity;

public abstract class BasaltEntity extends CraftEntity implements Cored {
    private final EntityCore core;

    protected BasaltEntity(EntityCore core) {
        this.core = core;
    }

    @Override
    public EntityCore core() {
        return core;
    }

    @Override
    public net.minecraft.world.entity.Entity getHandle() {
        return org.basaltmc.bukkit.EntityBridge.handle(core.world, core.id, core.uuid, core.type.getKey().toString());
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof Entity entity && entity.getUniqueId().equals(core.uuid);
    }

    @Override
    public int hashCode() {
        return core.uuid.hashCode();
    }

    @Override
    public String toString() {
        return getClass().getSimpleName() + "{id=" + core.id + "}";
    }
}
