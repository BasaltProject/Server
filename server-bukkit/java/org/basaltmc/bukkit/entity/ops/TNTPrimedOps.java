package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.TNTPrimed;

public interface TNTPrimedOps extends TNTPrimed, Cored {
    default int getFuseTicks() {
        return core().integer("fuse");
    }

    default void setFuseTicks(int ticks) {
        core().set("fuse", ticks);
    }
}
