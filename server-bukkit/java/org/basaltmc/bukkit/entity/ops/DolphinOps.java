package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Dolphin;

public interface DolphinOps extends Dolphin, Cored {
    default int getMoistness() {
        return core().nbtInt("Moistness");
    }

    default void setMoistness(int moistness) {
        core().nbtSet("Moistness", "int", moistness);
    }

    default boolean hasFish() {
        return core().nbtFlag("GotFish");
    }

    default void setHasFish(boolean fish) {
        core().nbtSet("GotFish", "bool", fish);
    }
}
