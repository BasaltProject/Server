package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Shearable;

public interface ShearableOps extends Shearable, Cored {
    default boolean isSheared() {
        return switch (core().type) {
            case SNOW_GOLEM -> core().nbtValue("Pumpkin") != null && !core().nbtFlag("Pumpkin");
            case BOGGED -> core().nbtFlag("sheared");
            default -> core().nbtFlag("Sheared");
        };
    }

    default void setSheared(boolean sheared) {
        switch (core().type) {
            case SNOW_GOLEM -> core().nbtSet("Pumpkin", "bool", !sheared);
            case BOGGED -> core().nbtSet("sheared", "bool", sheared);
            default -> core().nbtSet("Sheared", "bool", sheared);
        }
    }
}
