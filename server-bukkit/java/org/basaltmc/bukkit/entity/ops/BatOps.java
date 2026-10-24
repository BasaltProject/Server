package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Bat;

public interface BatOps extends Bat, Cored {
    default boolean isAwake() {
        return !core().flag("bat_resting");
    }

    default void setAwake(boolean awake) {
        core().call("bat_awake", awake);
    }

    default org.bukkit.Location getTargetLocation() {
        return MobPositions.parse(core(), core().text("bat_target"));
    }

    default void setTargetLocation(org.bukkit.Location location) {
        core().set("bat_target", location == null ? "" : MobPositions.format(location));
    }
}
