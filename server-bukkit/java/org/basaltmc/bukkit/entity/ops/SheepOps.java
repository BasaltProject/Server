package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Sheep;

public interface SheepOps extends Sheep, Cored {
    default boolean isSheared() {
        return core().nbtFlag("Sheared");
    }

    default void setSheared(boolean sheared) {
        core().nbtSet("Sheared", "bool", sheared);
    }
}
