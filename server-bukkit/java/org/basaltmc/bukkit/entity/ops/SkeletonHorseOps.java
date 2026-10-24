package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.SkeletonHorse;

public interface SkeletonHorseOps extends SkeletonHorse, Cored {
    default boolean isTrapped() {
        return core().flag("skeleton_trap");
    }

    default void setTrapped(boolean trap) {
        core().set("skeleton_trap", trap);
    }

    default int getTrapTime() {
        return core().integer("skeleton_trap_time");
    }

    default void setTrapTime(int ticks) {
        core().set("skeleton_trap_time", ticks);
    }
}
