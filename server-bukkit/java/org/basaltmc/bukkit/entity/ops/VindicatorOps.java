package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface VindicatorOps extends org.bukkit.entity.Vindicator, Cored {
    default boolean isJohnny() {
        return core().nbtFlag("Johnny");
    }

    default void setJohnny(boolean johnny) {
        core().nbtSet("Johnny", "bool", johnny);
    }
}
