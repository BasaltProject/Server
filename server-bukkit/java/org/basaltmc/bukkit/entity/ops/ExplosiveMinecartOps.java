package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.minecart.ExplosiveMinecart;

public interface ExplosiveMinecartOps extends ExplosiveMinecart, Cored {
    default int getFuseTicks() {
        return core().integer("fuse");
    }

    default void setFuseTicks(int ticks) {
        core().set("fuse", ticks);
    }

    default boolean isIgnited() {
        return core().integer("fuse") > -1;
    }

    default void ignite() {
        if (!isIgnited()) {
            core().set("fuse", 80);
        }
    }

    default float getExplosionSpeedFactor() {
        return (float) core().real("cart_explosion_speed");
    }

    default void setExplosionSpeedFactor(float factor) {
        core().set("cart_explosion_speed", (double) factor);
    }

    default void explode() {
        org.bukkit.util.Vector velocity = getVelocity();
        double speed = Math.min(5.0, velocity.getX() * velocity.getX() + velocity.getZ() * velocity.getZ());
        explode(speed);
    }

    default void explode(double speedSquared) {
        if (speedSquared < 0.0) {
            throw new IllegalArgumentException("Speed cannot be negative");
        }
        double power = 4.0 + java.util.concurrent.ThreadLocalRandom.current().nextDouble() * 1.5 * Math.min(5.0, speedSquared);
        org.bukkit.Location at = getLocation();
        remove();
        at.getWorld().createExplosion(at, (float) power, false, true, this);
    }
}
