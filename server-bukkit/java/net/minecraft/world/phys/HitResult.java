// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys;

import net.minecraft.world.entity.Entity;

public abstract class HitResult {
    protected final Vec3 location;

    protected HitResult(Vec3 location) {
        this.location = location;
    }

    public double distanceTo(Entity entity) {
        double dx = location.x - entity.getX();
        double dy = location.y - entity.getY();
        double dz = location.z - entity.getZ();
        return Math.sqrt(dx * dx + dy * dy + dz * dz);
    }

    public abstract Type getType();

    public Vec3 getLocation() {
        return location;
    }

    public static enum Type {
        MISS, BLOCK, ENTITY;
    }
}
