package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Ageable;

public interface PaperShearableOps extends io.papermc.paper.entity.Shearable, Cored {
    default boolean readyToBeSheared() {
        return switch (core().type) {
            case SHEEP -> !core().nbtFlag("Sheared") && ((Ageable) this).isAdult();
            case SNOW_GOLEM -> core().nbtFlag("Pumpkin");
            default -> this instanceof Ageable ageable && ageable.isAdult();
        };
    }
}
