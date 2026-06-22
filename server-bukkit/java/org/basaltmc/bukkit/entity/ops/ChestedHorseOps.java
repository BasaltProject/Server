package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.ChestedHorse;

public interface ChestedHorseOps extends ChestedHorse, Cored {
    default boolean isCarryingChest() {
        return core().nbtFlag("ChestedHorse");
    }

    default void setCarryingChest(boolean chest) {
        core().nbtSet("ChestedHorse", "bool", chest);
    }
}
