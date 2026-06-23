package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.IronGolem;

public interface IronGolemOps extends IronGolem, Cored {
    default boolean isPlayerCreated() {
        return core().nbtFlag("PlayerCreated");
    }

    default void setPlayerCreated(boolean created) {
        core().nbtSet("PlayerCreated", "bool", created);
    }
}
