package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.entity.EnderCrystal;

public interface EnderCrystalOps extends EnderCrystal, Cored {
    default boolean isShowingBottom() {
        return core().nbtValue("ShowBottom") == null || core().nbtFlag("ShowBottom");
    }

    default void setShowingBottom(boolean showing) {
        core().nbtSet("ShowBottom", "bool", showing);
    }

    default Location getBeamTarget() {
        return org.basaltmc.bukkit.entity.ops.EntityNbt.position(core().worldRef(),
            org.basaltmc.bukkit.entity.ops.EntityNbt.compound(core(), "").get("beam_target"));
    }

    default void setBeamTarget(Location location) {
        if (location == null) {
            core().nbtRemove("beam_target");
            return;
        }
        if (!core().worldRef().equals(location.getWorld())) {
            throw new IllegalArgumentException("Cannot set the beam target to another world");
        }
        core().nbtSet("beam_target", "pos", MobPositions.format(location));
    }
}
