package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Shearable;

public interface ShearableOps extends Shearable, Cored {
    default boolean isSheared() {
        return core().type == org.bukkit.entity.EntityType.SNOW_GOLEM ? !core().nbtFlag("Pumpkin")
            : core().nbtFlag("Sheared");
    }

    default void setSheared(boolean sheared) {
        if (core().type == org.bukkit.entity.EntityType.SNOW_GOLEM) {
            core().nbtSet("Pumpkin", "bool", !sheared);
        } else {
            core().nbtSet("Sheared", "bool", sheared);
        }
    }
}
