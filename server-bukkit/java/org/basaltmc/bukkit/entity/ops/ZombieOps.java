package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Zombie;

public interface ZombieOps extends Zombie, Cored {
    default boolean isBaby() {
        return core().integer("breed_age") < 0;
    }

    default void setBaby(boolean baby) {
        core().set("breed_age", baby ? -24000 : 0);
    }

    default int getConversionTime() {
        return core().nbtInt("DrownedConversionTime", -1);
    }

    default boolean canBreakDoors() {
        return core().nbtFlag("CanBreakDoors");
    }

    default void setCanBreakDoors(boolean breakDoors) {
        core().nbtSet("CanBreakDoors", "bool", breakDoors);
    }
}
