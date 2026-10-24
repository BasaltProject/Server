package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface PolarBearOps extends org.bukkit.entity.PolarBear, Cored {
    default boolean isStanding() {
        return core().flag("polar_standing");
    }

    default void setStanding(boolean standing) {
        core().set("polar_standing", standing);
    }
}
