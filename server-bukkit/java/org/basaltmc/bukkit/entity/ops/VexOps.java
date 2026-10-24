package org.basaltmc.bukkit.entity.ops;

import org.bukkit.Location;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Vex;

public interface VexOps extends Vex, Cored {
    default Location getBound() {
        return EntityNbt.position(core().worldRef(), EntityNbt.compound(core(), "").get("bound_pos"));
    }

    default void setBound(Location location) {
        if (location == null) {
            core().nbtRemove("bound_pos");
        } else {
            core().nbtSet("bound_pos", "pos",
                location.getBlockX() + "," + location.getBlockY() + "," + location.getBlockZ());
        }
    }

    default boolean hasLimitedLifetime() {
        return core().nbtValue("life_ticks") != null;
    }

    default void setLimitedLifetime(boolean limited) {
        if (limited) {
            if (!hasLimitedLifetime()) {
                core().nbtSet("life_ticks", "int", 0);
            }
        } else {
            core().nbtRemove("life_ticks");
        }
    }

    default int getLimitedLifetimeTicks() {
        return core().nbtInt("life_ticks");
    }

    default void setLimitedLifetimeTicks(int ticks) {
        core().nbtSet("life_ticks", "int", ticks);
    }

    default void setLifeTicks(int ticks) {
        setLimitedLifetimeTicks(ticks);
    }

    default boolean isCharging() {
        return core().flag("charging");
    }

    default void setCharging(boolean charging) {
        core().call("set_charging", charging);
    }

    default org.bukkit.entity.LivingEntity getOwner() {
        return core().entity(core().integer("owner_id")) instanceof org.bukkit.entity.LivingEntity owner ? owner : null;
    }

    default void setOwner(org.bukkit.entity.LivingEntity owner) {
        core().set("owner_id", owner == null ? -1 : owner.getEntityId());
    }
}
