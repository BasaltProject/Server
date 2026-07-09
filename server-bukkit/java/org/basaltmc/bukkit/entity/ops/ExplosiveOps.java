package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Explosive;

public interface ExplosiveOps extends Explosive, Cored {
    default float getYield() {
        return (float) core().real("explosion_power");
    }

    default void setYield(float yield) {
        core().set("explosion_power", (double) yield);
    }

    default boolean isIncendiary() {
        return core().flag("incendiary");
    }

    default void setIsIncendiary(boolean incendiary) {
        core().set("incendiary", incendiary);
    }
}
