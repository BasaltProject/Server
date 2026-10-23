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
}
