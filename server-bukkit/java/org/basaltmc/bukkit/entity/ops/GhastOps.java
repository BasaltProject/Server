package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Ghast;

public interface GhastOps extends Ghast, Cored {
    default int getExplosionPower() {
        return core().nbtInt("ExplosionPower", 1);
    }

    default void setExplosionPower(int power) {
        core().nbtSet("ExplosionPower", "byte", power);
    }
}
