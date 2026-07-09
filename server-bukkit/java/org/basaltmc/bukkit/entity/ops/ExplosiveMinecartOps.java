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
        explode(0.0);
    }

    default void explode(double power) {
        if (power < 0.0 || power > 5.0) {
            throw new IllegalArgumentException("Power must be in range [0, 5] (got " + power + ")");
        }
        core().set("cart_detonate", power);
    }
}
