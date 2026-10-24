package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Location;
import org.bukkit.entity.Creaking;
import org.bukkit.entity.Player;

public interface CreakingOps extends Creaking, Cored {
    default Location getHome() {
        return MobPositions.parse(core(), core().text("creaking_home"));
    }

    default void activate(Player player) {
        core().call("creaking_activate", (long) player.getEntityId());
    }

    default void deactivate() {
        core().call("creaking_deactivate");
    }

    default boolean isActive() {
        return core().flag("creaking_active");
    }
}
