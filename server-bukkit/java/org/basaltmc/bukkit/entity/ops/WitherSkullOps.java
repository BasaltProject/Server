package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface WitherSkullOps extends org.bukkit.entity.WitherSkull, Cored {
    default boolean isCharged() {
        return core().flag("wither_charged");
    }

    default void setCharged(boolean charged) {
        core().set("wither_charged", charged);
    }
}
