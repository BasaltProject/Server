package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Creeper;

public interface CreeperOps extends Creeper, Cored {
    int kMaxFuse = 30;

    default boolean isPowered() {
        return core().flag("powered");
    }

    default void setPowered(boolean powered) {
        core().set("powered", powered);
    }

    default int getFuseTicks() {
        return core().integer("swell");
    }

    default void setFuseTicks(int ticks) {
        core().set("swell", Math.max(0, ticks));
    }

    default int getMaxFuseTicks() {
        return kMaxFuse;
    }

    default boolean isIgnited() {
        return core().flag("creeper_ignited");
    }

    default void ignite() {
        core().set("creeper_ignited", true);
    }

    default void setIgnited(boolean ignited) {
        core().set("creeper_ignited", ignited);
        if (!ignited) {
            core().set("creeper_igniter", -1L);
        }
    }

    default org.bukkit.entity.Entity getIgniter() {
        return core().integer("creeper_igniter") >= 0 ? core().entity(core().integer("creeper_igniter")) : null;
    }

    default void explode() {
        setFuseTicks(kMaxFuse);
    }

    default int getExplosionRadius() {
        return core().nbtInt("ExplosionRadius", 3);
    }

    default void setExplosionRadius(int radius) {
        core().nbtSet("ExplosionRadius", "byte", radius);
    }

    default void setMaxFuseTicks(int ticks) {
        core().nbtSet("Fuse", "short", ticks);
    }

    default void ignite(org.bukkit.entity.Entity igniter) {
        core().set("creeper_igniter", igniter == null ? -1L : (long) igniter.getEntityId());
        ignite();
    }
}
