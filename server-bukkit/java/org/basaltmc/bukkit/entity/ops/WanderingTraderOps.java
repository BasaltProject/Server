package org.basaltmc.bukkit.entity.ops;

import org.bukkit.Location;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.WanderingTrader;

public interface WanderingTraderOps extends WanderingTrader, Cored {
    default int getDespawnDelay() {
        return core().nbtInt("DespawnDelay");
    }

    default void setDespawnDelay(int delay) {
        core().nbtSet("DespawnDelay", "int", delay);
    }

    default Location getWanderingTowards() {
        return EntityNbt.position(core().worldRef(), EntityNbt.compound(core(), "").get("wander_target"));
    }

    default void setWanderingTowards(Location location) {
        if (location == null) {
            core().nbtRemove("wander_target");
        } else {
            core().nbtSet("wander_target", "pos",
                location.getBlockX() + "," + location.getBlockY() + "," + location.getBlockZ());
        }
    }
}
